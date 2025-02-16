#pragma once

#include <nlohmann/json.hpp>

/// Interface that all stream handlers must implement.
class IStreamHandler {
public:
  virtual ~IStreamHandler() = default;

  /// Handle the data part of a combined stream event.
  virtual void handle(const nlohmann::json& data) const = 0;
};