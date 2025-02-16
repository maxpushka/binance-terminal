#pragma once

#include "stream_handler.h"

class OrderBookHandler final : public IStreamHandler {
 public:
  void handle(const nlohmann::json& data) const override;
};
