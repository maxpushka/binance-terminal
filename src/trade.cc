#include "trade.h"

#include <iostream>

#include "nlohmann/json.hpp"

struct Trade {
  double price;
  double quantity;
  uint64_t timestamp;
};

void from_json(const nlohmann::json& j, Trade& t) {
  t.price = std::stod(j.at("p").get<std::string>());
  t.quantity = std::stod(j.at("q").get<std::string>());
  j.at("T").get_to(t.timestamp);
}

[[nodiscard]] std::string TradeHandler::stream_name() const noexcept {
  return "aggTrade";
}

void TradeHandler::handle(const nlohmann::json& data) const {
  try {
    const Trade trade = data.get<Trade>();
    std::cout << "Trade: Price=" << trade.price
              << ", Quantity=" << trade.quantity << ", Time=" << trade.timestamp
              << '\n';
  } catch (const std::exception& e) {
    std::cerr << "JSON parse error: " << e.what() << '\n' << data << '\n';
  }
}
