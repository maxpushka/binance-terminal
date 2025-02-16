#include "orderbook.h"

#include <iostream>

#include "nlohmann/json.hpp"

std::string BinanceOrderBookWebSocketClient::get_subscription_streams() const {
  return symbol_ + "@depth";
}

void BinanceOrderBookWebSocketClient::process_message(
    const std::string& message) {
  try {
    using json = nlohmann::json;
    auto j = json::parse(message);
    const OrderBookUpdate update{
        j["U"].get<decltype(OrderBookUpdate::first_update_id)>(),
        j["u"].get<decltype(OrderBookUpdate::last_update_id)>(),
        j["b"].get<decltype(OrderBookUpdate::bids)>(),
        j["a"].get<decltype(OrderBookUpdate::asks)>(),
    };

    std::cout << "Order book update: " << update.first_update_id << " -> "
              << update.last_update_id << "(bids: " << update.bids.size()
              << ", asks: " << update.asks.size() << ")\n";
  } catch (const std::exception& e) {
    std::cerr << "JSON parse error: " << e.what() << '\n' << message << '\n';
  }
}
