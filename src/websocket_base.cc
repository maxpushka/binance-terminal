#include "websocket_base.h"
#include <boost/asio/connect.hpp>
#include <openssl/ssl.h>
#include <cstring> // for std::memcpy
#include <iostream>

BinanceWebSocketBase::BinanceWebSocketBase(asio::io_context& ioc, std::string symbol)
    : ssl_ctx_(asio::ssl::context::tlsv12_client),
      resolver_(ioc),
      ws_(ioc, ssl_ctx_),
      symbol_(std::move(symbol))
{
    ssl_ctx_.set_default_verify_paths(); // Use system's trusted CA certificates
}

void BinanceWebSocketBase::run() {
    resolver_.async_resolve("stream.binance.com", "9443",
        [this](auto ec, auto results) {
            if (ec) {
                std::cerr << "Resolve error: " << ec.message() << '\n';
                return;
            }
            connect(results);
        });
}

void BinanceWebSocketBase::connect(const asio::ip::tcp::resolver::results_type& endpoints) {
    asio::async_connect(ws_.next_layer().next_layer(), endpoints,
        [this](auto ec, auto) {
            if (ec) {
                std::cerr << "Connect error: " << ec.message() << '\n';
                return;
            }
            ws_.next_layer().async_handshake(asio::ssl::stream_base::client,
                [this](auto ec) { on_ssl_handshake(ec); });
        });
}

void BinanceWebSocketBase::on_ssl_handshake(boost::system::error_code ec) {
    if (ec) {
        std::cerr << "SSL Handshake error: " << ec.message() << '\n';
        return;
    }
    std::cout << "SSL handshake successful\n";

    // Set SNI hostname.
    SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), "stream.binance.com");

    // Build the WebSocket target based on the subscription streams.
    auto streams = get_subscription_streams();
    if (streams.empty()) {
        std::cerr << "No subscription streams provided!\n";
        return;
    }
    std::string target;
    if (streams.size() == 1) {
        target = "/ws/" + streams[0];
    } else {
        target = "/stream?streams=";
        for (size_t i = 0; i < streams.size(); ++i) {
            target += streams[i];
            if (i != streams.size() - 1) {
                target += "/";
            }
        }
    }

    ws_.async_handshake("stream.binance.com", target,
        [this](auto ec) { on_ws_handshake(ec); });
}

void BinanceWebSocketBase::on_ws_handshake(boost::system::error_code ec) {
    if (ec) {
        std::cerr << "WebSocket Handshake error: " << ec.message() << '\n';
        return;
    }
    std::cout << "WebSocket handshake successful\n";
    read();
}

void BinanceWebSocketBase::read() {
    ws_.async_read(buffer_,
        [this](auto ec, std::size_t bytes_transferred) {
            if (ec) {
                std::cerr << "Read error: " << ec.message() << '\n';
                return;
            }

            std::string message;
            message.resize(bytes_transferred);
            std::memcpy(&message[0], buffer_.data().data(), bytes_transferred);
            buffer_.consume(bytes_transferred);
            process_message(message);
            read();  // Continue reading
        });
}
