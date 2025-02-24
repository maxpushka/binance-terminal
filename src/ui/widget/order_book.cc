module;
#include <ranges>
#include <mutex>

#include "ftxui/component/component.hpp"
#include "rpp/subjects/publish_subject.hpp"

module ui.widget;

import state;

namespace widget {
class OrderBookSideBody final : public component::TableBody<
      state::OrderBookSide, state::OrderBookEntry> {
public:
  OrderBookSideBody(
      const publish_subject<state::OrderBookSide>& order_book_input,
      publish_subject<component::RedrawSignal>& output)
    : TableBody(order_book_input, output) {
  }

  void ProcessEvent(const state::OrderBookSide& event) override {
    std::lock_guard lock(mutex_);
    events_.clear();
    events_ = std::ranges::to<std::vector>(event | std::views::values);
  }

  RowType BuildRow(const state::OrderBookEntry& entry) const override {
    const double total = entry.price * entry.quantity;
    return {
        std::to_string(entry.price),
        std::to_string(entry.quantity),
        total > 1000
          ? std::format("{:.2f}K", total / 1000)
          : std::to_string(total),
    };
  }
};

class OrderBookMidPrice final : public ftxui::ComponentBase {
public:
  OrderBookMidPrice(const publish_subject<state::OrderBook>& order_book_input,
                    const publish_subject<state::Trade>&
                    quote_to_usdt_trades_input,
                    publish_subject<component::RedrawSignal>& output)
    : update_subject_{output} {
    order_book_input.get_observable().subscribe(
        [this](const state::OrderBook& ob) {
          if (ob.bids.empty() && ob.asks.empty()) {
            return; // TODO: handle case where there are no bids or asks.
          }
          {
            // For bids, the best (highest) price is at the end of the map.
            const double best_bid = ob.bids.rbegin()->first;
            // For asks, the best (lowest) price is at the beginning of the map.
            const double best_ask = ob.asks.begin()->first;

            std::lock_guard lock(mutex_);
            prev_mid_price = mid_price_;
            mid_price_ = (best_bid + best_ask) / 2.0;
          }
          update_subject_.get_observer().on_next(component::RedrawSignal{});
        });
    quote_to_usdt_trades_input.get_observable().subscribe(
        [this](const state::Trade& trade) {
          std::lock_guard lock(mutex_);
          mark_price_ = trade.price * mid_price_;
          update_subject_.get_observer().on_next(component::RedrawSignal{});
        });
  }

  ftxui::Element Render() override {
    std::lock_guard lock(mutex_);

    // Choose color and arrow direction based on price movement.
    constexpr std::array colors = {ftxui::Color::Green, ftxui::Color::Red,};
    constexpr std::array arrows = {ftxui::Color::Green, ftxui::Color::Red,};
    const bool is_up = mid_price_ > prev_mid_price;
    const auto color = colors[is_up];
    const auto arrow = arrows[is_up];

    // Draw order book mid-price
    ftxui::Canvas c{80, 25}; // TODO: compute canvas size dynamically.
    const int current_x = DrawNumber(c, mid_price_, 0, 0, color);
    DrawDigit(c, arrow, current_x, 0, color);

    return ftxui::hbox({
        ftxui::canvas(c),
        ftxui::text(std::to_wstring(mark_price_)),
    });
  }

private:
  // Convert a floating-point number to a string and draw each character in pixel-art style.
  static int DrawNumber(ftxui::Canvas& canvas, const float number,
                 const int offset_x = 0,
                 const int offset_y = 0, const int spacing = 1,
                 const ftxui::Color& color = ftxui::Color::White) {
    const std::string num_str = std::to_string(number);
    int current_x = offset_x;
    for (const char ch : num_str) {
      DrawDigit(canvas, ch, current_x, offset_y, color);
      // Each digit is 3 columns wide; add extra spacing between digits.
      current_x += 3 + spacing;
    }
    return current_x;
  }

  // Draw a single digit (or dot) on the canvas starting at (offset_x, offset_y).
  // For each non-space pixel in our pattern, we use DrawPoint to “turn on” that pixel
  // with a blue color.
  static void DrawDigit(ftxui::Canvas& canvas, const char digit, const int offset_x,
                 const int offset_y, const ftxui::Color& color) {
    const auto it = DIGITS.find(digit);
    if (it == DIGITS.end()) return; // unsupported character.
    const auto& pattern = it->second;
    for (size_t y = 0; y < pattern.size(); ++y) {
      for (size_t x = 0; x < pattern[y].size(); ++x) {
        if (pattern[y][x] == ' ') continue;
        canvas.DrawPoint(offset_x + x, offset_y + y, true, color);
      }
    }
  }

  mutable std::mutex mutex_;
  publish_subject<component::RedrawSignal>& update_subject_;
  double mark_price_ = 0.0;
  double mid_price_ = 0.0;
  double prev_mid_price = 0.0;

  // A 5x3 pixel-art patterns for digits 0–9 and the decimal point.
  static constexpr char arrow_up = '^';
  static constexpr char arrow_down = 'v';
  static const std::unordered_map<char, std::array<std::string, 5>> DIGITS;
};

const std::unordered_map<char, std::array<std::string, 5>> OrderBookMidPrice::DIGITS = {
  {'0', {"###", "# #", "# #", "# #", "###"}},
  {'1', {"  #", "  #", "  #", "  #", "  #"}},
  {'2', {"###", "  #", "###", "#  ", "###"}},
  {'3', {"###", "  #", "###", "  #", "###"}},
  {'4', {"# #", "# #", "###", "  #", "  #"}},
  {'5', {"###", "#  ", "###", "  #", "###"}},
  {'6', {"###", "#  ", "###", "# #", "###"}},
  {'7', {"###", "  #", "  #", "  #", "  #"}},
  {'8', {"###", "# #", "###", "# #", "###"}},
  {'9', {"###", "# #", "###", "  #", "###"}},
  {'.', {"   ", "   ", "   ", " ##", " ##"}},
  {arrow_up, {" # ", "###", " # ", " # ", " # "}},
  {arrow_down, {" # ", " # ", " # ", "###", " # "}}
};

// OrderBook function constructs the order book widget
// with the following structure:
//
// White text  : Order Book
// Table header: Price (USDT), Amount (BTC), Time
// Table body  : --asks--
// Mid-price   : 123.45↑  $456.67
// Table body  : --bids--
ftxui::Component OrderBook(
    const publish_subject<state::OrderBook>& order_book_input,
    const publish_subject<state::Trade>& quote_to_usdt_trades_input,
    const publish_subject<std::vector<std::string>>& header_input,
    publish_subject<component::RedrawSignal>& output) {
  // Split the order book into two sides: asks and bids.
  publish_subject<state::OrderBookSide> asks_subject;
  publish_subject<state::OrderBookSide> bids_subject;
  order_book_input.get_observable().subscribe(
      [&asks_subject, &bids_subject](const state::OrderBook& ob) {
        asks_subject.get_observer().on_next(ob.asks);
        bids_subject.get_observer().on_next(ob.bids);
      });

  // Instantiate header and body components first.
  auto header = std::make_shared<component::TableHeader>(header_input, output);
  auto asks = std::make_shared<OrderBookSideBody>(asks_subject, output);
  auto bids = std::make_shared<OrderBookSideBody>(bids_subject, output);
  auto mid_price = std::make_shared<OrderBookMidPrice>(order_book_input,
    quote_to_usdt_trades_input,
    output);

  // Create the widget
  return ftxui::Renderer([&header, &asks, &mid_price, &bids] {
    return ftxui::vbox({
        ftxui::text(L"Order Book"),
        header->Render(),
        asks->Render(),
        mid_price->Render(),
        bids->Render(),
    });
  });
}
} // namespace widget
