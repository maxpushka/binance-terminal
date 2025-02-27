#include <ranges>

#include "boost/asio/co_spawn.hpp"
#include "boost/asio/detached.hpp"
#include "boost/asio/io_context.hpp"
#include "rpp/subjects/publish_subject.hpp"
#include "spdlog/spdlog.h"

import exchange;
import state;

int main() {
  boost::asio::io_context io_context;
  exchange::WebSocketStreams ws(io_context);
  exchange::WebSocketAPI api(io_context);
  boost::asio::co_spawn(io_context, ws.run(), boost::asio::detached);
  boost::asio::co_spawn(io_context, api.run(), boost::asio::detached);

  auto handler = std::make_unique<state::OrderBookHandler>(api);
  const auto subject = handler->get_subject();
  boost::asio::co_spawn(
      io_context,
      [&ws, &handler] -> boost::asio::awaitable<void> {
        co_await ws.subscribe("btcusdt", std::move(handler));
        co_return;
      },
      boost::asio::detached);

  subject.get_observable().subscribe([](const state::OrderBook& trade) {
    // Fetch top N bids and asks levels
    // without materializing the whole map into values
    constexpr auto N = 5;
    namespace v = std::views;
    const auto bids = trade.bids | v::take(N) | v::values;
    const auto asks = trade.asks | v::take(N) | v::values;

    // Print the top bids and asks levels
    std::stringstream ss;
    ss << "Bids: ";
    for (const auto& [price, quantity] : bids) {
      ss << std::format("{:.2f}@{:.2f} ", quantity, price);
    }
    ss << "Asks: ";
    for (const auto& [price, quantity] : asks) {
      ss << std::format("{:.2f}@{:.2f} ", quantity, price);
    }
    spdlog::info(ss.str());
  });

  io_context.run();
  return 0;
}
