#pragma once

#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <string>
#include <unordered_map>

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

/// Self-sufficient Binance WebSocket client (final; not intended for
/// inheritance).
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
  asio::awaitable<void>
  list_subscriptions();  // TODO: return std::vector<std::string>

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

  std::unordered_map<std::string, std::unique_ptr<IStreamHandler>> handlers_;
  mutable std::shared_mutex handlers_mutex_;
};
