#pragma once

#include "websocket_base.h"

// A simple Trade structure.
struct Trade {
  double price;
  double quantity;
  uint64_t timestamp;
};

class BinanceTradeWebSocketClient : public BinanceWebSocketBase {
 public:
  // Inherit the constructor.
  using BinanceWebSocketBase::BinanceWebSocketBase;

  // Specify the handshake target for trade streams.
  [[nodiscard]] std::string get_subscription_streams() const;

 protected:
  // Process a trade message.
  void process_message(const std::string& message) override;
};
