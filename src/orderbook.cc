#include "orderbook.h"

#include <iostream>

#include "nlohmann/json.hpp"

void OrderBookHandler::handle(const nlohmann::json& data) const {
  try {
    const OrderBookUpdate update{
        data["U"].get<decltype(OrderBookUpdate::first_update_id)>(),
        data["u"].get<decltype(OrderBookUpdate::last_update_id)>(),
        data["b"].get<decltype(OrderBookUpdate::bids)>(),
        data["a"].get<decltype(OrderBookUpdate::asks)>(),
    };

    std::cout << "Order book update: " << update.first_update_id << " -> "
              << update.last_update_id << "(bids: " << update.bids.size()
              << ", asks: " << update.asks.size() << ")\n";
  } catch (const std::exception& e) {
    std::cerr << "JSON parse error: " << e.what() << '\n' << data << '\n';
  }
}
