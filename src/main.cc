#include <iostream>

#include "orderbook.h"
#include "trade.h"

int main() {
  try {
    const std::string market = "btcusdt";

    asio::io_context ioc;
    BinanceTradeWebSocketClient trades(ioc, market);
    BinanceOrderBookWebSocketClient orderbook(ioc, market);

    // Schedule the asynchronous run tasks.
    boost::asio::co_spawn(ioc, trades.run(), boost::asio::detached);
    boost::asio::co_spawn(ioc, orderbook.run(), boost::asio::detached);

    // Schedule subscriptions if needed.
    boost::asio::co_spawn(ioc,
                          trades.subscribe(trades.get_subscription_streams()),
                          boost::asio::detached);
    boost::asio::co_spawn(
        ioc, orderbook.subscribe(orderbook.get_subscription_streams()),
        boost::asio::detached);

    // Process events.
    ioc.run();
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << '\n';
  }
}
