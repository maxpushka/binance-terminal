#pragma once

#include <nlohmann/json.hpp>

/// Interface that all stream handlers must implement.
class IStreamHandler {
public:
  virtual ~IStreamHandler() = default;

  /// Get the name of the stream this handler is responsible for.
  [[nodiscard]] virtual std::string stream_name() const noexcept = 0;

  /// Handle the data part of a combined stream event.
  virtual void handle(const nlohmann::json& data) const = 0;
};
