#include "websocket_streams.h"

#include <chrono>
#include <future>
#include <iostream>

using namespace boost::asio;
using namespace boost::beast;

WebSocketStreams::WebSocketStreams(asio::io_context& ioc)
    : WebSocket(ioc, "stream.binance.com", "9443", "/stream") {}

/// Subscribe to a stream:
/// - Registers the handler (under a write lock).
/// - Sends the SUBSCRIBE command.
asio::awaitable<void> WebSocketStreams::subscribe(
    const std::string& market, std::unique_ptr<IStreamHandler> handler) {
  const std::string stream = market + "@" + handler->stream_name();

  {
    // Write-lock the handlers map to register the new handler.
    std::unique_lock lock(stream_handlers_mutex_);
    if (stream_handlers_.contains(stream)) {
      std::cerr << "Stream " << stream << " is already subscribed.\n";
      co_return;
    }
    stream_handlers_.emplace(stream, std::move(handler));
  }

  co_await wait_for_connection();
  const int id = next_request_id_.fetch_add(1, std::memory_order_relaxed);

  // Set up a promise/future to wait for the subscription confirmation.
  auto prom = std::make_shared<std::promise<nlohmann::json>>();
  auto fut = prom->get_future();
  {
    std::unique_lock lock(request_handlers_mutex_);
    request_handlers_[id] = [prom](const nlohmann::json& response) {
      prom->set_value(response);
    };
  }

  const std::string json = R"({"method": "SUBSCRIBE", "params": [")" + stream +
                           R"("], "id": )" + std::to_string(id) + "}";
  std::cout << "Subscribing to " << stream << " (" << id << ")...\n";
  co_await send_json(json);

  const auto executor = co_await asio::this_coro::executor;
  // Poll until the future is ready.
  while (fut.wait_for(std::chrono::milliseconds(0)) !=
         std::future_status::ready) {
    steady_timer timer(executor, std::chrono::milliseconds(10));
    co_await timer.async_wait(use_awaitable);
  }

  {
    // Ensure the request handler is removed regardless of the outcome.
    std::unique_lock lock(request_handlers_mutex_);
    request_handlers_.erase(id);
  }

  if (nlohmann::json response = fut.get();
      !response.contains("result") || !response["result"].is_null()) {
    std::cerr << "Subscription failed for stream: " << stream << "\n";
    // Remove the handler if subscription confirmation fails.
    std::unique_lock lock(stream_handlers_mutex_);
    stream_handlers_.erase(stream);
  }
  std::cout << "Subscribed to " << stream << " (" << id << ")...\n";
  co_return;
}

/// Unsubscribe from a stream:
/// - Removes the handler (under a write lock).
/// - Sends the UNSUBSCRIBE command.
asio::awaitable<void> WebSocketStreams::unsubscribe(const std::string& stream) {
  {
    std::unique_lock lock(stream_handlers_mutex_);
    const auto it = stream_handlers_.find(stream);
    if (it == stream_handlers_.end()) {
      std::cerr << "Stream " << stream << " is not subscribed.\n";
      co_return;
    }
    stream_handlers_.erase(it);
  }

  co_await wait_for_connection();
  const int id = next_request_id_.fetch_add(1, std::memory_order_relaxed);

  // Set up a promise/future to wait for the unsubscription confirmation.
  auto prom = std::make_shared<std::promise<nlohmann::json>>();
  auto fut = prom->get_future();
  {
    std::unique_lock lock(request_handlers_mutex_);
    request_handlers_[id] = [prom](const nlohmann::json& response) {
      prom->set_value(response);
    };
  }

  const std::string json = R"({"method": "UNSUBSCRIBE", "params": [")" +
                           stream + R"("], "id": )" + std::to_string(id) + "}";
  std::cout << "Unsubscribing from " << stream << " (" << id << ")...\n";
  co_await send_json(json);

  const auto executor = co_await asio::this_coro::executor;
  // Poll until the future is ready.
  while (fut.wait_for(std::chrono::milliseconds(0)) !=
         std::future_status::ready) {
    steady_timer timer(executor, std::chrono::milliseconds(10));
    co_await timer.async_wait(use_awaitable);
  }

  {
    // Ensure the request handler is removed regardless of the outcome.
    std::unique_lock lock(request_handlers_mutex_);
    request_handlers_.erase(id);
  }

  if (nlohmann::json response = fut.get();
      !response.contains("result") || !response["result"].is_null()) {
    std::cerr << "Unsubscription failed for stream: " << stream << "\n";
  }
  std::cout << "Unsubscribed from " << stream << " (" << id << ")...\n";
  co_return;
}

/// List active subscriptions.
/// Sends the LIST_SUBSCRIPTIONS command and waits for a response in the format:
///   {"result": ["btcusdt@aggTrade"], "id": <id>}
asio::awaitable<std::vector<std::string>>
WebSocketStreams::list_subscriptions() {
  const auto executor = co_await asio::this_coro::executor;
  co_await wait_for_connection();
  const int id = next_request_id_.fetch_add(1, std::memory_order_relaxed);

  // Prepare a promise/future pair to wait for the response.
  auto prom = std::make_shared<std::promise<nlohmann::json>>();
  auto fut = prom->get_future();

  {
    std::unique_lock lock(request_handlers_mutex_);
    request_handlers_[id] = [prom](const nlohmann::json& response) {
      prom->set_value(response);
    };
  }

  const std::string json =
      R"({"method": "LIST_SUBSCRIPTIONS", "id": )" + std::to_string(id) + "}";
  std::cout << "Listing subscriptions (" << id << ")...\n";
  co_await send_json(json);

  // Since std::future doesn't support co_await, poll the future.
  while (fut.wait_for(std::chrono::milliseconds(0)) !=
         std::future_status::ready) {
    steady_timer timer(executor, std::chrono::milliseconds(10));
    co_await timer.async_wait(use_awaitable);
  }
  nlohmann::json response = fut.get();

  if (!response.contains("result")) {
    std::cerr << "Response does not contain 'result' field.\n";
    co_return std::vector<std::string>{};
  }

  // Extract the vector of subscriptions from the "result" field.
  auto subscriptions = response["result"].get<std::vector<std::string>>();
  co_return subscriptions;
}

/// Process an incoming message by first checking for stream events (hot path)
/// and then for request events.
void WebSocketStreams::process_message(const std::string& message) {
  try {
    auto j = nlohmann::json::parse(message);

    // First, process stream events if present.
    if (j.contains("stream") && j.contains("data")) {
      const std::string streamName = j["stream"].get<std::string>();
      const auto& data = j["data"];

      // Use a shared (read) lock on the handlers map.
      std::shared_lock lock(stream_handlers_mutex_);
      if (const auto it = stream_handlers_.find(streamName);
          it != stream_handlers_.end()) {
        it->second->handle(data);
      } else {
        std::cerr << "No handler registered for stream: " << streamName << "\n";
      }
    }

    // Then process request events if present.
    if (j.contains("id")) {
      const int id = j["id"].get<int>();
      // Taking a unique (write) lock here since we modify the map.
      std::unique_lock lock(request_handlers_mutex_);
      if (const auto it = request_handlers_.find(id);
          it != request_handlers_.end()) {
        // Retrieve and remove the handler.
        const auto handler = std::move(it->second);
        request_handlers_.erase(it);
        lock.unlock();
        handler(j);
      }
    }

    std::cout << "Unknown message: " << j << "\n";
  } catch (const std::exception& ex) {
    std::cerr << "Error parsing message: " << ex.what() << "\n";
  }
}
