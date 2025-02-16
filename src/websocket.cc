#include "websocket.h"

#include <openssl/ssl.h>

#include <boost/asio/connect.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <iostream>

using namespace boost::asio;
using namespace boost::beast;

BinanceWebSocket::BinanceWebSocket(asio::io_context& ioc)
    : ssl_ctx_(asio::ssl::context::tlsv12_client),
      resolver_(ioc),
      ws_(ioc, ssl_ctx_) {
  ssl_ctx_.set_default_verify_paths();  // Use system's trusted CA certificates.
}

asio::awaitable<void> BinanceWebSocket::wait_for_connection() const {
  auto executor = co_await this_coro::executor;
  while (!connected_) {
    // Wait briefly before checking again.
    steady_timer timer(executor, std::chrono::milliseconds(50));
    co_await timer.async_wait(use_awaitable);
  }
  co_return;
}

asio::awaitable<void> BinanceWebSocket::establish_connection() {
  try {
    auto executor = co_await this_coro::executor;
    const auto url = "stream.binance.com";

    // Resolve the hostname.
    auto endpoints =
        co_await resolver_.async_resolve(url, "9443", use_awaitable);

    // Connect to one of the resolved endpoints.
    co_await async_connect(ws_.next_layer().next_layer(), endpoints,
                           use_awaitable);

    // Perform the SSL handshake.
    co_await ws_.next_layer().async_handshake(asio::ssl::stream_base::client,
                                              use_awaitable);

    // Set SNI hostname.
    SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), url);

    // Perform the WebSocket handshake.
    co_await ws_.async_handshake(url, "/stream", use_awaitable);

    // Set up a control callback to handle ping frames.
    ws_.control_callback([this](boost::beast::websocket::frame_type kind,
                                boost::string_view payload) {
      if (kind == boost::beast::websocket::frame_type::ping) {
        try {
          // Construct a ping_data object from the payload.
          const boost::beast::websocket::ping_data pd{std::string(payload)};
          ws_.pong(pd);
          std::cout << "Ping received, pong sent.\n";
        } catch (const std::exception& e) {
          std::cerr << "Error sending pong: " << e.what() << "\n";
        }
      }
    });

    // Mark connection as established.
    connected_ = true;
    std::cout << "Connection established!\n";
  } catch (std::exception& e) {
    std::cerr << "Exception in establish_connection: " << e.what() << "\n";
  }
  co_return;
}

asio::awaitable<void> BinanceWebSocket::run() {
  co_await establish_connection();
  while (true) {
    flat_buffer buffer;
    co_await ws_.async_read(buffer, use_awaitable);
    std::string message = buffers_to_string(buffer.data());
    process_message(message);
  }
}

asio::awaitable<void> BinanceWebSocket::send_json(const std::string& message) {
  co_await ws_.async_write(boost::asio::buffer(message), use_awaitable);
  co_return;
}

/// Subscribe to a stream:
/// - Registers the handler (under a write lock).
/// - Sends the SUBSCRIBE command.
asio::awaitable<void> BinanceWebSocket::subscribe(
    const std::string& stream, std::unique_ptr<IStreamHandler> handler) {
  {
    // Write-lock the handlers map to register the new handler.
    std::unique_lock lock(handlers_mutex_);
    if (handlers_.contains(stream)) {
      std::cerr << "Stream " << stream << " is already subscribed.\n";
      co_return;
    }
    handlers_.emplace(stream, std::move(handler));
  }

  co_await wait_for_connection();
  const int id = next_request_id_.fetch_add(1, std::memory_order_relaxed);
  const std::string json = R"({"method": "SUBSCRIBE", "params": [")" + stream +
                           R"("], "id": )" + std::to_string(id) + "}";
  std::cout << "Subscribing to " << stream << " (" << id << ")...\n";
  co_await send_json(json);
}

/// Unsubscribe from a stream:
/// - Removes the handler (under a write lock).
/// - Sends the UNSUBSCRIBE command.
asio::awaitable<void> BinanceWebSocket::unsubscribe(const std::string& stream) {
  {
    std::unique_lock lock(handlers_mutex_);
    const auto it = handlers_.find(stream);
    if (it == handlers_.end()) {
      std::cerr << "Stream " << stream << " is not subscribed.\n";
      co_return;
    }
    handlers_.erase(it);
  }

  co_await wait_for_connection();
  const int id = next_request_id_.fetch_add(1, std::memory_order_relaxed);
  const std::string json = R"({"method": "UNSUBSCRIBE", "params": [")" +
                           stream + R"("], "id": )" + std::to_string(id) + "}";
  std::cout << "Unsubscribing from " << stream << " (" << id << ")...\n";
  co_await send_json(json);
}

/// List active subscriptions.
asio::awaitable<void> BinanceWebSocket::list_subscriptions() {
  co_await wait_for_connection();
  const int id = next_request_id_.fetch_add(1, std::memory_order_relaxed);
  const std::string json =
      R"({"method": "LIST_SUBSCRIPTIONS", "id": )" + std::to_string(id) + "}";
  std::cout << "Listing subscriptions (" << id << ")...\n";
  co_await send_json(json);
  co_return;
}

/// Process an incoming message by parsing the JSON, extracting the "stream"
/// and "data" fields, and dispatching the data to the appropriate handler.
void BinanceWebSocket::process_message(const std::string& message) {
  try {
    // Expecting a combined stream event:
    // {"stream": "<streamName>", "data": <rawPayload>}
    if (auto j = nlohmann::json::parse(message);
        j.contains("stream") && j.contains("data")) {
      const std::string streamName = j["stream"].get<std::string>();
      const auto& data = j["data"];

      // Use a shared lock to safely read the handler.
      std::shared_lock lock(handlers_mutex_);
      if (const auto it = handlers_.find(streamName); it != handlers_.end()) {
        it->second->handle(data);
      } else {
        std::cerr << "No handler registered for stream: " << streamName << "\n";
      }
    } else {
      std::cerr
          << "Invalid message format: missing 'stream' or 'data' fields: `"
          << message << "`\n";
    }
  } catch (const std::exception& ex) {
    std::cerr << "Error parsing message: " << ex.what() << "\n";
  }
}
