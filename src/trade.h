#pragma once

#include "stream_handler.h"

class TradeHandler final : public IStreamHandler {
 public:
  void handle(const nlohmann::json& data) const override;
};
