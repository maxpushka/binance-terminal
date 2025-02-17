#pragma once

#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <nlohmann/json.hpp>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;

class WebSocket {
 public:
  WebSocket(asio::io_context& ioc, const std::string& host,
            const std::string& port, const std::string& target);
  virtual ~WebSocket() = default;

  // Coroutine-based entry point.
  // TODO: run this method in ctor
  asio::awaitable<void> run();

 protected:
  // Wait until the connection is established.
  asio::awaitable<void> wait_for_connection() const;

  // Helper to send a JSON command message.
  asio::awaitable<void> send_json(const std::string& message);

  // Process each incoming message.
  virtual void process_message(const std::string& message) = 0;

 private:
  std::string host_, port_, target_;
  asio::ssl::context ssl_ctx_;
  asio::ip::tcp::resolver resolver_;
  websocket::stream<beast::ssl_stream<asio::ip::tcp::socket>> ws_;
  std::atomic_bool connected_{false};

  asio::awaitable<void> establish_connection();
};
