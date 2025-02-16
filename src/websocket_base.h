#pragma once

#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <string>
#include <utility>
#include <vector>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;

// Base class using C++23 coroutines for WebSocket handling.
class BinanceWebSocketBase {
 public:
  explicit BinanceWebSocketBase(asio::io_context& ioc, std::string symbol);
  virtual ~BinanceWebSocketBase() = default;

  // Coroutine-based entry point.
  asio::awaitable<void> run();

  // Live subscription methods (coroutine versions).
  asio::awaitable<void> subscribe(const std::string& stream);
  asio::awaitable<void> unsubscribe(const std::string& stream);

  // List currently active subscriptions.
  asio::awaitable<void> list_subscriptions();

 protected:
  // Process each incoming message (e.g. handle data or errors).
  virtual void process_message(const std::string& message) = 0;

  // Helper to send a JSON command message.
  asio::awaitable<void> send_json(const std::string& message);

  asio::ssl::context ssl_ctx_;
  asio::ip::tcp::resolver resolver_;
  websocket::stream<beast::ssl_stream<asio::ip::tcp::socket>> ws_;
  std::string symbol_;

 private:
  asio::awaitable<void> wait_for_connection() const;
  asio::awaitable<void> establish_connection();

  // Atomic request id generator for thread safety.
  std::atomic<int> next_request_id_{1};
  std::atomic_bool connected_{false};
};
