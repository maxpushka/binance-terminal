#pragma once

#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <functional>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;

/// Interface that all stream handlers must implement.
class IStreamHandler {
 public:
  virtual ~IStreamHandler() = default;

  /// Handle the data part of a combined stream event.
  virtual void handle(const nlohmann::json& data) const = 0;
};

/// Self-sufficient Binance WebSocket client.
class BinanceWebSocket final {
 public:
  explicit BinanceWebSocket(asio::io_context& ioc);
  ~BinanceWebSocket() = default;

  // Coroutine-based entry point.
  asio::awaitable<void> run();

  /// Subscribe to a stream:
  /// - Registers the handler for the given stream.
  /// - Sends the SUBSCRIBE command.
  asio::awaitable<void> subscribe(const std::string& stream,
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
  void process_message(const std::string& message);

  // Helper to send a JSON command message.
  asio::awaitable<void> send_json(const std::string& message);

  asio::ssl::context ssl_ctx_;
  asio::ip::tcp::resolver resolver_;
  websocket::stream<beast::ssl_stream<asio::ip::tcp::socket>> ws_;

 private:
  asio::awaitable<void> wait_for_connection() const;
  asio::awaitable<void> establish_connection();

  std::atomic<int> next_request_id_{1};
  std::atomic_bool connected_{false};

  // Stream handlers (hot path).
  std::unordered_map<std::string, std::unique_ptr<IStreamHandler>>
      stream_handlers_;
  mutable std::shared_mutex stream_handlers_mutex_;
  // Request handlers.
  std::unordered_map<int, std::function<void(const nlohmann::json&)>>
      request_handlers_;
  mutable std::shared_mutex request_handlers_mutex_;
};
