#pragma once

#include "websocket.h"

// A simple Trade structure.
struct Trade {
  double price;
  double quantity;
  uint64_t timestamp;
};

class TradeHandler final : public IStreamHandler {
 public:
  void handle(const nlohmann::json& data) const override;
};
