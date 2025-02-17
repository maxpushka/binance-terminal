module;
#include <atomic>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <future>
#include <iostream>
#include <nlohmann/json.hpp>
#include <regex>

module exchange;

void from_json(const nlohmann::json& j, OrderBookSnapshot& obs) {
  j.at("lastUpdateId").get_to(obs.last_update_id);
  obs.bids = j.at("bids").get<decltype(OrderBookSnapshot::bids)>();
  obs.asks = j.at("asks").get<decltype(OrderBookSnapshot::asks)>();
}

WebsocketAPI::WebsocketAPI(boost::asio::io_context& io_context)
    : WebSocket(io_context, "ws-api.binance.com", "443", "/ws-api/v3") {}

[[nodiscard]] asio::awaitable<OrderBookSnapshot>
WebsocketAPI::get_orderbook_snapshot(const std::string& market) {
  // Validate market using a regex.
  if (const std::regex market_regex("^[a-zA-Z0-9-_.]{1,20}$");
      !std::regex_match(market, market_regex)) {
    throw std::invalid_argument("Invalid market symbol: " + market);
  }

  // Convert to upper case
  std::string uppercaseMarket;
  for (const char c : market) {
    uppercaseMarket.push_back(std::toupper(c));
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

  const std::string json = R"({"method": "depth", "params": {"symbol":")" +
                           uppercaseMarket + R"(","limit":5000}, "id": )" +
                           std::to_string(id) + "}";
  std::cout << "Requesting order book snapshot for " << uppercaseMarket << " ("
            << id << ")...\n";
  co_await send_json(json);

  const auto executor = co_await asio::this_coro::executor;
  // Poll until the future is ready.
  while (fut.wait_for(std::chrono::milliseconds(0)) !=
         std::future_status::ready) {
    asio::steady_timer timer(executor, std::chrono::milliseconds(10));
    co_await timer.async_wait(asio::use_awaitable);
  }

  {
    // Ensure the request handler is removed regardless of the outcome.
    std::unique_lock lock(request_handlers_mutex_);
    request_handlers_.erase(id);
  }

  // Parse the response.
  nlohmann::json response = fut.get();
  if (!response.contains("result") || response["result"].is_null()) {
    std::cerr << "Failed to fetch order book snapshot for '" << uppercaseMarket
              << "': " << response << "\n";
    throw std::runtime_error("failed to fetch order book snapshot");
  }
  nlohmann::json& result = response["result"];
  auto snapshot = result.get<OrderBookSnapshot>();
  std::cout << "Fetched order book snapshot for " << uppercaseMarket << " ("
            << id << ")...\n";
  co_return snapshot;
}

void WebsocketAPI::process_message(const std::string& message) {
  try {
    const auto j = nlohmann::json::parse(message);

    // Then process request events if present.
    if (!j.contains("id")) {
      std::cerr << "Unknown WS API message: " << j << "\n";
      return;
    }

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
  } catch (const std::exception& ex) {
    std::cerr << "Error parsing message: " << ex.what() << "\n";
  }
}