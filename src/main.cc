#include <iostream>

#include "orderbook.h"
#include "trade.h"
#include "websocket.h"

int main() {
  boost::asio::io_context io_context;

  // Create the Binance WebSocket client for a given symbol.
  BinanceWebSocket ws(io_context);

  boost::asio::co_spawn(
      io_context,
      [&ws]() -> boost::asio::awaitable<void> {
        const std::string market = "btcusdt";
        co_await ws.subscribe(market + "@aggTrade",
                              std::make_unique<TradeHandler>());
        co_await ws.subscribe(market + "@depth",
                              std::make_unique<OrderBookHandler>());
      },
      boost::asio::detached);

  // Launch the WebSocket connection run loop.
  co_spawn(io_context, ws.run(), boost::asio::detached);

  io_context.run();
  return 0;
}
