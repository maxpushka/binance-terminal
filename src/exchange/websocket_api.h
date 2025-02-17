#pragma once

#include <shared_mutex>

#include "websocket.h"

struct OrderBookSnapshot {
  int64_t last_update_id;
  std::vector<std::array<std::string, 2>> bids;
  std::vector<std::array<std::string, 2>> asks;
};

void from_json(const nlohmann::json& j, OrderBookSnapshot& obs);

class WebsocketAPI final : public WebSocket {
 public:
  explicit WebsocketAPI(boost::asio::io_context& io_context);

  [[nodiscard]] asio::awaitable<OrderBookSnapshot> get_orderbook_snapshot(
      const std::string& market);

 protected:
  void process_message(const std::string& message) override;

 private:
  std::atomic<int> next_request_id_{1};

  // Request handlers.
  std::unordered_map<int, std::function<void(const nlohmann::json&)>>
      request_handlers_;
  mutable std::shared_mutex request_handlers_mutex_;
};
