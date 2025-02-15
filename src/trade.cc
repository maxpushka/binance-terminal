#include "trade.h"

#include <iostream>

#include "nlohmann/json.hpp"

std::vector<std::string> BinanceTradeWebSocketClient::get_subscription_streams()
    const {
  return {symbol_ + "@aggTrade"};
}

void BinanceTradeWebSocketClient::process_message(const std::string& message) {
  try {
    using json = nlohmann::json;
    auto j = json::parse(message);
    Trade trade{std::stod(j["p"].get<std::string>()),
                std::stod(j["q"].get<std::string>()), j["T"].get<uint64_t>()};

    std::cout << "Trade: Price=" << trade.price
              << ", Quantity=" << trade.quantity << ", Time=" << trade.timestamp
              << '\n';
  } catch (const std::exception& e) {
    std::cerr << "JSON parse error: " << e.what() << '\n';
  }
}
