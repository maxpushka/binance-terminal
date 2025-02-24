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
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

import exchange;
import state;
import ui.component;
import ui.widget;

using namespace std::chrono_literals;
using namespace ftxui;

int main() {
  auto file_sink = spdlog::basic_logger_mt("logger", "logs/basic-log.txt");
  spdlog::set_default_logger(std::move(file_sink));

  boost::asio::io_context io_context;

  // Instantiate Binance client.
  exchange::WebSocketStreams ws(io_context);
  exchange::WebSocketAPI api(io_context);
  // Start the IO coroutines.
  boost::asio::co_spawn(io_context, ws.run(), boost::asio::detached);
    boost::asio::co_spawn(io_context, api.run(), boost::asio::detached);

  // Set up stream handlers.
  auto trade_handler = std::make_unique<state::TradeHandler>();
  auto order_book_handler = std::make_unique<state::OrderBookHandler>(api);

  // WARN: moving the following subject fetching lines below the co_spawn will cause invalid reference error.
  const auto& trade_subject = trade_handler->get_subject();
  const auto& order_book_subject = order_book_handler->get_subject();

  // Subscribe to the market data stream.
  boost::asio::co_spawn(
      io_context,
      [&ws, &trade_handler, &order_book_handler] -> boost::asio::awaitable<void> {
        constexpr auto market = "btcusdt";
        co_await ws.subscribe(market, std::move(trade_handler));
        co_await ws.subscribe(market, std::move(order_book_handler));
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
