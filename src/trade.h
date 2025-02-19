#pragma once
#include <string>

#include "nlohmann/json.hpp"
#include "rpp/subjects/publish_subject.hpp"

import exchange;

struct Trade {
  double price;
  double quantity;
  uint64_t timestamp;
};

inline void from_json(const nlohmann::json& j, Trade& t);

// Use ReactivePlusPlus's publish_subject as our global event bus.
inline rpp::subjects::publish_subject<Trade>& get_trade_subject() {
  static rpp::subjects::publish_subject<Trade> subject;
  return subject;
}

struct TradeHandler final : IStreamHandler {
  [[nodiscard]] std::string stream_name() const noexcept override;
  void handle(const nlohmann::json& data) const override;
};
