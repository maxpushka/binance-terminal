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
  boost::asio::co_spawn(io_context, ws.run(), boost::asio::detached);

  auto handler = std::make_unique<state::TradeHandler>();
  const auto subject = handler->get_subject();
  boost::asio::co_spawn(
      io_context,
      [&ws, &handler] -> boost::asio::awaitable<void> {
        co_await ws.subscribe("btcusdt", std::move(handler));
        co_return;
      },
      boost::asio::detached);

  subject.get_observable().subscribe([](const state::Trade& trade) {
    spdlog::info(
        "event_time={}"
        " symbol={}"
        " trade_id={}"
        " price={}"
        " quantity={}"
        " first_trade_id={}"
        " last_trade_id={}"
        " trade_time={}"
        " is_buyer_market_maker={}",
        std::format("{:%T}", trade.event_time), trade.symbol, trade.trade_id,
        trade.price, trade.quantity, trade.first_trade_id, trade.last_trade_id,
        std::format("{:%T}", trade.trade_time), trade.is_buyer_market_maker);
  });

  io_context.run();
  return 0;
}
