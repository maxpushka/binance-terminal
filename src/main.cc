#include <iostream>
#include "trade.h"
#include "orderbook.h"

int main() {
    try {
        const std::string market = "btcusdt";

        asio::io_context ioc;
        BinanceTradeWebSocketClient trades(ioc, market);
        BinanceOrderBookWebSocketClient orderbook(ioc, market);

        // Start async operations.
        trades.run();
        orderbook.run();

        // Process events.
        ioc.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }
}
