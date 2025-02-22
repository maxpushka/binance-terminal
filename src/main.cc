#include <iostream>
#include <mutex>
#include <thread>

#include "boost/asio/awaitable.hpp"
#include "boost/asio/co_spawn.hpp"
#include "boost/asio/detached.hpp"
#include "boost/asio/io_context.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "rpp/subjects/publish_subject.hpp"

import exchange;
import state;
import ui.component;
import ui.widget;

using namespace std::chrono_literals;
using namespace ftxui;

int main() {
  boost::asio::io_context io_context;

  // Instantiate your Binance WebSocket client.
  exchange::WebSocketStreams ws(io_context);
  // Start the websocket run coroutine.
  boost::asio::co_spawn(io_context, ws.run(), boost::asio::detached);

  // Stream market data.
  auto trade_handler = std::make_unique<state::TradeHandler>();
  const auto& trade_subject =
      trade_handler
          ->get_subject();  // WARN: moving this line below the co_spawn will
                            // cause invalid reference error.
  boost::asio::co_spawn(
      io_context,
      [&ws, &trade_handler] -> boost::asio::awaitable<void> {
        co_await ws.subscribe("btcusdt", std::move(trade_handler));
        co_return;
      },
      boost::asio::detached);

  // Set up UI components.
  const rpp::subjects::publish_subject<std::vector<std::string>> header_subject;
  rpp::subjects::publish_subject<component::RedrawSignal> redraw_subject;
  const auto market_trades =
      widget::MarketTrades(trade_subject, header_subject, redraw_subject);

  // Set up event handlers.
  auto screen = ScreenInteractive::TerminalOutput();
  header_subject.get_observer().on_next(
      {"Price (USDT)", "Amount (BTC)", "Time"});
  redraw_subject.get_observable().subscribe(
      [&screen](component::RedrawSignal) { screen.PostEvent(Event::Custom); });
  const auto shutdown_handler = [&screen, &io_context] {
    screen.Exit();
    io_context.stop();
  };
  const auto ui_handler =
      CatchEvent(market_trades, [&shutdown_handler](const Event& event) {
        if (event == Event::Custom) return true;
        if (event == Event::Escape) {
          shutdown_handler();
          return true;
        }
        return false;
      });

  // Run IO & UI loops.
  std::thread io_thread([&io_context] { io_context.run(); });
  screen.Loop(ui_handler);  // run the UI loop in the main thread.

  // Clean up.
  shutdown_handler();  // in case user hits Ctrl+C instead of ESC.
  io_thread.join();
  return EXIT_SUCCESS;
}
