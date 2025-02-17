#include <iostream>

#include "exchange/websocket_api.h"
#include "exchange/websocket_streams.h"
#include "orderbook.h"
#include "trade.h"

int main() {
  boost::asio::io_context io_context;

  // Create the Binance WebSocket clients for streams and API.
  WebSocketStreams ws(io_context);
  WebsocketAPI api(io_context);
  co_spawn(io_context, ws.run(), boost::asio::detached);
  co_spawn(io_context, api.run(), boost::asio::detached);

  constexpr auto market = "btcusdt";

  boost::asio::co_spawn(
      io_context,
      [&api, &market]() -> boost::asio::awaitable<void> {
        OrderBookSnapshot snapshot;
        try {
          snapshot = co_await api.get_orderbook_snapshot(market);
        } catch (const std::exception& e) {
          std::cerr << "Error: " << e.what() << std::endl;
          co_return;
        }
        std::stringstream ss;
        ss << "Order book snapshot for " << market << ":\n";
        for (const auto& bid : snapshot.bids) {
          ss << "  " << bid[0] << " @ " << bid[1] << "\n";
        }
        std::cout << ss.str() << std::endl;
      },
      boost::asio::detached);

  boost::asio::co_spawn(
      io_context,
      [&ws, &market]() -> boost::asio::awaitable<void> {
        // Subscribe to markets
        co_await ws.subscribe(market, std::make_unique<TradeHandler>());
        co_await ws.subscribe(market, std::make_unique<OrderBookHandler>());

        // List active subscriptions.
        const auto subscriptions = co_await ws.list_subscriptions();
        std::stringstream ss;
        ss << "Subscribed to streams: ";
        for (const auto& stream : subscriptions) {
          ss << stream << ", ";
        }
        std::cout << ss.str() << std::endl;
      },
      boost::asio::detached);

  io_context.run();
  return 0;
}
