#include "websocket_base.h"

#include <openssl/ssl.h>

#include <boost/asio/connect.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <iostream>

using namespace boost::asio;
using namespace boost::beast;

BinanceWebSocketBase::BinanceWebSocketBase(asio::io_context& ioc,
                                           std::string symbol)
    : ssl_ctx_(asio::ssl::context::tlsv12_client),
      resolver_(ioc),
      ws_(ioc, ssl_ctx_),
      symbol_(std::move(symbol)) {
  ssl_ctx_.set_default_verify_paths();  // Use system's trusted CA certificates.
}

asio::awaitable<void> BinanceWebSocketBase::wait_for_connection() const {
  auto executor = co_await boost::asio::this_coro::executor;
  while (!connected_) {
    // Wait briefly before checking again.
    boost::asio::steady_timer timer(executor, std::chrono::milliseconds(50));
    co_await timer.async_wait(boost::asio::use_awaitable);
  }
  co_return;
}

asio::awaitable<void> BinanceWebSocketBase::establish_connection() {
  try {
    auto executor = co_await boost::asio::this_coro::executor;
    const auto url = "stream.binance.com";

    // Resolve the hostname.
    auto endpoints = co_await resolver_.async_resolve(
        url, "9443", boost::asio::use_awaitable);

    // Connect to one of the resolved endpoints.
    co_await boost::asio::async_connect(ws_.next_layer().next_layer(),
                                        endpoints, boost::asio::use_awaitable);

    // Perform the SSL handshake.
    co_await ws_.next_layer().async_handshake(ssl::stream_base::client,
                                              boost::asio::use_awaitable);

    // Set SNI hostname.
    SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), url);

    // Perform the WebSocket handshake.
    co_await ws_.async_handshake(url, "/stream", boost::asio::use_awaitable);

    // Set up a control callback to handle ping frames.
    ws_.control_callback([this](boost::beast::websocket::frame_type kind,
                                boost::string_view payload) {
      if (kind == boost::beast::websocket::frame_type::ping) {
        try {
          // Construct a ping_data object from the payload.
          boost::beast::websocket::ping_data pd{std::string(payload)};
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

asio::awaitable<void> BinanceWebSocketBase::run() {
  co_await establish_connection();
  while (true) {
    beast::flat_buffer buffer;
    co_await ws_.async_read(buffer, use_awaitable);
    std::string message = buffers_to_string(buffer.data());
    process_message(message);
  }
}

asio::awaitable<void> BinanceWebSocketBase::send_json(
    const std::string& message) {
  co_await ws_.async_write(boost::asio::buffer(message), use_awaitable);
  co_return;
}

asio::awaitable<void> BinanceWebSocketBase::subscribe(
    const std::string& stream) {
  co_await wait_for_connection();

  // Build a vector of market stream strings.
  const int id = next_request_id_.fetch_add(1, std::memory_order_relaxed);
  std::string json = R"({"method": "SUBSCRIBE", "params": [)";
  json += "\"" + stream + "\"";
  json += "], \"id\": " + std::to_string(id) + "}";
  std::cout << "Subscribing to " << stream << " (" << id << ")...\n";
  co_await send_json(json);
}

asio::awaitable<void> BinanceWebSocketBase::unsubscribe(
    const std::string& stream) {
  co_await wait_for_connection();

  const int id = next_request_id_.fetch_add(1, std::memory_order_relaxed);
  std::string json = R"({"method": "UNSUBSCRIBE", "params": [)";
  json += "\"" + stream + "\"";
  json += "], \"id\": " + std::to_string(id) + "}";
  std::cout << "Unsubscribing from " << stream << " (" << id << ")...\n";
  co_await send_json(json);
}

asio::awaitable<void> BinanceWebSocketBase::list_subscriptions() {
  co_await wait_for_connection();

  const int id = next_request_id_.fetch_add(1, std::memory_order_relaxed);
  const std::string json =
      R"({"method": "LIST_SUBSCRIPTIONS", "id": )" + std::to_string(id) + "}";
  std::cout << "Listing subscriptions (" << id << ")...\n";
  co_await send_json(json);
  co_return;
}
