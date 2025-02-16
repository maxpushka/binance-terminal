#pragma once

#include "websocket.h"

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

class OrderBookHandler final : public IStreamHandler {
 public:
  void handle(const nlohmann::json& data) const override;
};
