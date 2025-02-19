#include <iostream>
#include <mutex>
#include <thread>

#include "boost/asio/awaitable.hpp"
#include "boost/asio/co_spawn.hpp"
#include "boost/asio/detached.hpp"
#include "boost/asio/io_context.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

import exchange;
import state;

using namespace std::chrono_literals;
using namespace ftxui;

int main() {
  boost::asio::io_context io_context;

  // Instantiate your Binance WebSocket client.
  WebSocketStreams ws(io_context);
  // Start the websocket run coroutine.
  boost::asio::co_spawn(io_context, ws.run(), boost::asio::detached);

  // Subscribe to the trade stream for "btcusdt".
  boost::asio::co_spawn(
      io_context,
      [&ws]() -> boost::asio::awaitable<void> {
        co_await ws.subscribe("btcusdt", std::make_unique<TradeHandler>());
        co_return;
      },
      boost::asio::detached);

  // Set up FTXUI.
  auto screen = ScreenInteractive::TerminalOutput();
  std::mutex trade_mutex;
  Trade latest_trade{};

  // Subscribe to Trade events via the ReactivePlusPlus subject.
  const auto trade_observable = get_trade_subject().get_observable();
  trade_observable.subscribe(
      [&](const Trade& t) {
        {
          std::lock_guard lock(trade_mutex);
          latest_trade = t;
        }
        // Notify FTXUI to refresh the UI.
        screen.PostEvent(Event::Custom);
      },
      [](const std::exception_ptr& ep) {
        try {
          if (ep) std::rethrow_exception(ep);
        } catch (const std::exception& e) {
          std::cerr << "Trade observable error: " << e.what() << "\n";
        }
      });

  // FTXUI renderer: display the latest trade.
  const auto ui_renderer = Renderer([&] {
    std::lock_guard lock(trade_mutex);
    return vbox({
               text("Latest Trade:"),
               text("Price: " + std::to_string(latest_trade.price)),
               text("Quantity: " + std::to_string(latest_trade.quantity)),
               text("Timestamp: " + std::to_string(latest_trade.timestamp)),
               separator(),
               text("Press ESC to exit"),
           }) |
           center;
  });

  // Component to catch custom events (and exit on ESC).
  const auto main_component = CatchEvent(ui_renderer, [&](const Event& event) {
    if (event == Event::Custom) return true;
    if (event == Event::Escape) {
      screen.Exit();  // gracefully exit the event loop.
      io_context.stop();
      return true;
    }
    return false;
  });

  std::thread io_thread([&io_context] { io_context.run(); });
  screen.Loop(main_component);  // run the UI loop in the main thread.
  io_thread.join();

  return EXIT_SUCCESS;
}
