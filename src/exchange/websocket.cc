module;
#include <iostream>
#include <shared_mutex>

#include "boost/asio.hpp"
#include "boost/asio/awaitable.hpp"
#include "boost/asio/ssl.hpp"
#include "boost/beast/core/buffers_to_string.hpp"
#include "boost/beast/core/flat_buffer.hpp"
#include "boost/beast/ssl.hpp"
#include "boost/beast/websocket/ssl.hpp"
#include "boost/beast/websocket/stream.hpp"
#include "boost/utility/string_view_fwd.hpp"
#include "nlohmann/json.hpp"
#include "openssl/ssl.h"
#include "spdlog/spdlog.h"

module exchange;

namespace exchange {
namespace asio = boost::asio;
namespace beast = boost::beast;

WebSocket::WebSocket(asio::io_context& ioc, const std::string& host,
                     const std::string& port, const std::string& target)
    : host_(host),
      port_(port),
      target_(target),
      ssl_ctx_(asio::ssl::context::tlsv12_client),
      resolver_(ioc),
      ws_(ioc, ssl_ctx_) {
  ssl_ctx_.set_default_verify_paths();  // Use system's trusted CA certificates.
}

asio::awaitable<void> WebSocket::wait_for_connection() const {
  const auto executor = co_await asio::this_coro::executor;
  while (!connected_) {
    // Wait briefly before checking again.
    asio::steady_timer timer(executor, std::chrono::milliseconds(50));
    co_await timer.async_wait(asio::use_awaitable);
  }
  co_return;
}

asio::awaitable<void> WebSocket::establish_connection() {
  try {
    auto executor = co_await asio::this_coro::executor;

    // Resolve the hostname.
    auto endpoints =
        co_await resolver_.async_resolve(host_, port_, asio::use_awaitable);

    // Connect to one of the resolved endpoints.
    co_await async_connect(ws_.next_layer().next_layer(), endpoints,
                           asio::use_awaitable);

    // Perform the SSL handshake.
    co_await ws_.next_layer().async_handshake(asio::ssl::stream_base::client,
                                              asio::use_awaitable);

    // Set SNI hostname.
    SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host_.c_str());

    // Perform the WebSocket handshake.
    co_await ws_.async_handshake(host_, target_, asio::use_awaitable);

    // Set up a control callback to handle ping frames.
    ws_.control_callback([this](const boost::beast::websocket::frame_type kind,
                                const boost::string_view payload) {
      if (kind == boost::beast::websocket::frame_type::ping) {
        try {
          // Construct a ping_data object from the payload.
          const boost::beast::websocket::ping_data pd{std::string(payload)};
          ws_.pong(pd);
          spdlog::info("ping received, pong sent");
        } catch (const std::exception& e) {
          spdlog::error("Error sending pong: {}", e.what());
        }
      }
    });

    // Mark connection as established.
    connected_ = true;
    spdlog::info("connection established");
  } catch (std::exception& e) {
    spdlog::error("exception in establish_connection: {}", e.what());
  }
  co_return;
}

asio::awaitable<void> WebSocket::run() {
  co_await establish_connection();
  while (true) {
    beast::flat_buffer buffer;
    co_await ws_.async_read(buffer, asio::use_awaitable);
    std::string message = beast::buffers_to_string(buffer.data());
    co_await process_message(message);
  }
}

asio::awaitable<void> WebSocket::send_json(const std::string& message) {
  co_await ws_.async_write(boost::asio::buffer(message), asio::use_awaitable);
  co_return;
}
}  // namespace exchange
