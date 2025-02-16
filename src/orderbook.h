#pragma once

#include "websocket_base.h"

struct OrderBookSnapshot {
  int64_t last_update_id;
  std::vector<std::vector<std::string>> bids;
  std::vector<std::vector<std::string>> asks;
};

struct OrderBookUpdate {
  int64_t first_update_id;
  int64_t last_update_id;
  std::vector<std::vector<std::string>> bids;
  std::vector<std::vector<std::string>> asks;
};

class BinanceOrderBookWebSocketClient : public BinanceWebSocketBase {
 public:
  // Inherit the constructor.
  using BinanceWebSocketBase::BinanceWebSocketBase;

  // Specify the handshake target for order book streams.
  [[nodiscard]] std::string get_subscription_streams() const;

 protected:
  // Process an order book message.
  void process_message(const std::string& message) override;
};
