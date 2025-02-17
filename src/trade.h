#pragma once
#include <nlohmann/json.hpp>
#include <string>

import exchange;

struct TradeHandler final : IStreamHandler {
  [[nodiscard]] std::string stream_name() const noexcept override;
  void handle(const nlohmann::json& data) const override;
};
