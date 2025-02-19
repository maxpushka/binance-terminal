#include "trade.h"

#include <iostream>

inline void from_json(const nlohmann::json& j, Trade& t) {
  t.price = std::stod(j.at("p").get<std::string>());
  t.quantity = std::stod(j.at("q").get<std::string>());
  j.at("T").get_to(t.timestamp);
}

std::string TradeHandler::stream_name() const noexcept { return "aggTrade"; }

void TradeHandler::handle(const nlohmann::json& data) const {
  Trade trade{};
  try {
    trade = data.get<Trade>();
  } catch (const std::exception& e) {
    std::cerr << "JSON parse error in TradeHandler: " << e.what() << "\n"
              << data << "\n";
  }
  get_trade_subject().get_observer().on_next(trade);
}
