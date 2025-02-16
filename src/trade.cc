#include "trade.h"

#include <iostream>

#include "nlohmann/json.hpp"

void TradeHandler::handle(const nlohmann::json& data) const {
  try {
    const Trade trade{std::stod(data["p"].get<std::string>()),
                      std::stod(data["q"].get<std::string>()),
                      data["T"].get<decltype(Trade::timestamp)>()};

    std::cout << "Trade: Price=" << trade.price
              << ", Quantity=" << trade.quantity << ", Time=" << trade.timestamp
              << '\n';
  } catch (const std::exception& e) {
    std::cerr << "JSON parse error: " << e.what() << '\n' << data << '\n';
  }
}
