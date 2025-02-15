#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <vector>
#include <string>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;

class BinanceWebSocketBase {
public:
    explicit BinanceWebSocketBase(asio::io_context& ioc, std::string symbol);
    virtual ~BinanceWebSocketBase() = default;

    // Start the asynchronous connection process.
    void run();

protected:
    // Derived classes should return the subscription stream names without prefixes.
    // For example, for a trade stream, return { symbol_ + "@aggTrade" }.
    virtual std::vector<std::string> get_subscription_streams() const = 0;

    // Process each incoming message.
    virtual void process_message(const std::string& message) = 0;

    // The following functions implement the async connection, handshake, and read flow.
    void connect(const asio::ip::tcp::resolver::results_type& endpoints);
    void on_ssl_handshake(boost::system::error_code ec);
    void on_ws_handshake(boost::system::error_code ec);
    void read();

    asio::ssl::context ssl_ctx_;
    asio::ip::tcp::resolver resolver_;
    websocket::stream<beast::ssl_stream<asio::ip::tcp::socket>> ws_;
    beast::flat_buffer buffer_;
    std::string symbol_;
};
