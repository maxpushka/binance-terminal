module;

#include <format>

#include "ftxui/component/component.hpp"
#include "rpp/subjects/publish_subject.hpp"

module ui.widget;

import state;
import ui.component;

namespace widget {
class MarketTradesBody final : public component::TableBody<state::Trade> {
 public:
  MarketTradesBody(const publish_subject<state::Trade>& trade_input,
                   publish_subject<component::RedrawSignal>& output)
      : TableBody(trade_input, output) {}

  [[nodiscard]] RowType BuildRow(const state::Trade& trade) const override {
    return {
        std::format("{:.5f}", trade.price),
        std::format("{:.5f}", trade.quantity),
        std::to_string(trade.event_time),
    };
  }

  [[nodiscard]] ftxui::Element RenderCell(
      const std::string& cell, const size_t width, const size_t col_index,
      const state::Trade& trade) const override {
    using namespace ftxui;
    if (col_index == 0) {
      const Color price_color =
          trade.is_buyer_market_maker ? Color::Green : Color::Red;
      return text(cell) | size(WIDTH, EQUAL, static_cast<int>(width)) |
             color(price_color);
    }
    return text(cell) | size(WIDTH, EQUAL, static_cast<int>(width)) |
           color(Color::White);
  }
};

ftxui::Component MarketTrades(
    const publish_subject<state::Trade>& trade_subject,
    const publish_subject<std::vector<std::string>>& header_subject,
    publish_subject<component::RedrawSignal>& redraw_subject) {
  // Instantiate header and body components first.
  auto header_component =
      std::make_shared<component::TableHeader>(header_subject, redraw_subject);
  auto body_component =
      std::make_shared<MarketTradesBody>(trade_subject, redraw_subject);
  // Create the composite scrollable table.
  return std::make_shared<component::ScrollableTable>(
      header_component, body_component, redraw_subject);
}
}  // namespace widget
