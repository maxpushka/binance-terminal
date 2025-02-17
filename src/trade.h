#pragma once

#include "exchange/websocket_streams_handler.h"

struct TradeHandler final : IStreamHandler {
  [[nodiscard]] std::string stream_name() const noexcept override;
  void handle(const nlohmann::json& data) const override;
};
