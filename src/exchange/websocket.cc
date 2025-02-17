#include "websocket.h"

#include <openssl/ssl.h>

#include <boost/asio/connect.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <iostream>

using namespace boost::asio;
using namespace boost::beast;

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
  const auto executor = co_await this_coro::executor;
  while (!connected_) {
    // Wait briefly before checking again.
    steady_timer timer(executor, std::chrono::milliseconds(50));
    co_await timer.async_wait(use_awaitable);
  }
  co_return;
}

asio::awaitable<void> WebSocket::establish_connection() {
  try {
    auto executor = co_await this_coro::executor;

    // Resolve the hostname.
    auto endpoints =
        co_await resolver_.async_resolve(host_, port_, use_awaitable);

    // Connect to one of the resolved endpoints.
    co_await async_connect(ws_.next_layer().next_layer(), endpoints,
                           use_awaitable);

    // Perform the SSL handshake.
    co_await ws_.next_layer().async_handshake(asio::ssl::stream_base::client,
                                              use_awaitable);

    // Set SNI hostname.
    SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host_.c_str());

    // Perform the WebSocket handshake.
    co_await ws_.async_handshake(host_, target_, use_awaitable);

    // Set up a control callback to handle ping frames.
    ws_.control_callback([this](const boost::beast::websocket::frame_type kind,
                                const boost::string_view payload) {
      if (kind == boost::beast::websocket::frame_type::ping) {
        try {
          // Construct a ping_data object from the payload.
          const boost::beast::websocket::ping_data pd{std::string(payload)};
          ws_.pong(pd);
          std::cout << "Ping received, pong sent.\n";
        } catch (const std::exception& e) {
          std::cerr << "Error sending pong: " << e.what() << "\n";
        }
      }
    });

    // Mark connection as established.
    connected_ = true;
    std::cout << "Connection established!\n";
  } catch (std::exception& e) {
    std::cerr << "Exception in establish_connection: " << e.what() << "\n";
  }
  co_return;
}

asio::awaitable<void> WebSocket::run() {
  co_await establish_connection();
  while (true) {
    flat_buffer buffer;
    co_await ws_.async_read(buffer, use_awaitable);
    std::string message = buffers_to_string(buffer.data());
    process_message(message);
  }
}

asio::awaitable<void> WebSocket::send_json(const std::string& message) {
  co_await ws_.async_write(boost::asio::buffer(message), use_awaitable);
  co_return;
}
