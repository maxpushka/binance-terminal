#pragma once

#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/beast/websocket.hpp>
#include <functional>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "websocket.h"
#include "websocket_streams_handler.h"

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;

/// Self-sufficient Binance WebSocket client.
class WebSocketStreams final : public WebSocket {
 public:
  explicit WebSocketStreams(asio::io_context& ioc);
  ~WebSocketStreams() override = default;

  /// Subscribe to a stream:
  /// - Registers the handler for the given stream.
  /// - Sends the SUBSCRIBE command.
  asio::awaitable<void> subscribe(const std::string& market,
                                  std::unique_ptr<IStreamHandler> handler);

  /// Unsubscribe from a stream:
  /// - Removes the handler for the given stream.
  /// - Sends the UNSUBSCRIBE command.
  asio::awaitable<void> unsubscribe(const std::string& stream);

  /// List active subscriptions.
  /// - Sends the LIST_SUBSCRIPTIONS command.
  /// - Returns a vector of subscribed stream names.
  asio::awaitable<std::vector<std::string>> list_subscriptions();

 protected:
  // Process each incoming message.
  void process_message(const std::string& message) override;

 private:
  std::atomic<int> next_request_id_{1};

  // Stream handlers (hot path).
  std::unordered_map<std::string, std::unique_ptr<IStreamHandler>>
      stream_handlers_;
  mutable std::shared_mutex stream_handlers_mutex_;
  // Request handlers.
  std::unordered_map<int, std::function<void(const nlohmann::json&)>>
      request_handlers_;
  mutable std::shared_mutex request_handlers_mutex_;
};
