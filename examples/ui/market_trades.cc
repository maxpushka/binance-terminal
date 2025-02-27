#include <ranges>

#include "boost/asio/co_spawn.hpp"
#include "boost/asio/detached.hpp"
#include "boost/asio/io_context.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "rpp/subjects/publish_subject.hpp"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"

import exchange;
import state;
import ui.component;
import ui.widget;

int main() {
  // Set up logging.
  auto now = std::chrono::system_clock::now();
  auto file_sink_tag =
      std::format("logs/ui_market_trades_{0:%F}T{0:%R%z}.log", now);
  auto file_sink = spdlog::basic_logger_mt("logger", file_sink_tag);
  spdlog::set_default_logger(std::move(file_sink));

  // Set up IO components.
  boost::asio::io_context io_context;
  exchange::WebSocketStreams ws(io_context);
  boost::asio::co_spawn(io_context, ws.run(), boost::asio::detached);

  constexpr std::string_view base = "btc";
  constexpr std::string_view quote = "usdt";

  auto handler = std::make_unique<state::TradeHandler>();
  const auto subject = handler->get_subject();
  boost::asio::co_spawn(
      io_context,
      [&ws, &base,&quote, &handler] -> boost::asio::awaitable<void> {
        co_await ws.subscribe(std::format("{}{}", base, quote),
                              std::move(handler));
        co_return;
      },
      boost::asio::detached);

  // Set up UI components.
  const rpp::subjects::publish_subject<std::vector<std::string>> header_subject;
  rpp::subjects::publish_subject<component::RedrawSignal> redraw_subject;
  const auto market_trades =
      widget::MarketTrades(subject, header_subject, redraw_subject);

  // Set up event handlers.

  // Set up header.
  namespace r = std::ranges;
  namespace v = r::views;
  const auto base_header = base | v::transform([](const char c) { return std::toupper(c); }) |
      r::to<std::string>();
  const auto quote_header = quote | v::transform([](const char c) { return std::toupper(c); }) |
      r::to<std::string>();
  header_subject.get_observer().on_next({
        std::format("Price ({})", quote_header),
        std::format("Amount ({})", base_header),
    "Time",
  });

  // Set up screen.
  auto screen = ftxui::ScreenInteractive::TerminalOutput();
  redraw_subject.get_observable().subscribe([&screen](component::RedrawSignal) {
    screen.PostEvent(ftxui::Event::Custom);
  });
  const auto shutdown_handler = [&screen, &io_context] {
    screen.Exit();
    io_context.stop();
  };
  const auto ui_handler =
      CatchEvent(market_trades, [&shutdown_handler](const ftxui::Event& event) {
        if (event == ftxui::Event::Custom) return true;
        if (event == ftxui::Event::Escape) {
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
  return 0;
}
