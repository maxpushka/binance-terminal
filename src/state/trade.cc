module;
#include "nlohmann/json.hpp"
#include "spdlog/spdlog.h"

module state;

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
    spdlog::error("JSON parse error: {} (`{}`)", e.what(), data.dump());
    return;
  }
  get_trade_subject().get_observer().on_next(trade);
}
