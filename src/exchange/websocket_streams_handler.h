#pragma once
#include <string>

#include "nlohmann/json.hpp"

namespace exchange {
/// Interface that all stream handlers must implement.
struct IStreamHandler {
  virtual ~IStreamHandler() = default;
  /// Get the name of the stream this handler is responsible for.
  [[nodiscard]] virtual std::string stream_name() const noexcept = 0;
  /// Handle the data part of a combined stream event.
  virtual void handle(const nlohmann::json& data) const = 0;
};
}  // namespace exchange
