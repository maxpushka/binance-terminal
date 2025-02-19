#pragma once
#include <string>

#include "nlohmann/json.hpp"

import exchange;

struct OrderBookHandler final : IStreamHandler {
  [[nodiscard]] std::string stream_name() const noexcept override;
  void handle(const nlohmann::json& data) const override;
};
