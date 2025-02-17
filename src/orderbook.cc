#include "orderbook.h"

#include <iostream>

#include "nlohmann/json.hpp"

struct OrderBookUpdate {
  int64_t first_update_id;
  int64_t last_update_id;
  std::vector<std::vector<std::string>> bids;
  std::vector<std::vector<std::string>> asks;
  uint64_t timestamp;
};

void from_json(const nlohmann::json& j, OrderBookUpdate& obu) {
  // {
  //   "e": "depthUpdate", // Event type
  //   "E": 1672515782136, // Event time
  //   "s": "BNBBTC",      // Symbol
  //   "U": 157,           // First update ID in event
  //   "u": 160,           // Final update ID in event
  //   "b": [              // Bids to be updated
  //     [
  //       "0.0024",       // Price level to be updated
  //       "10"            // Quantity
  //     ]
  //   ],
  //   "a": [              // Asks to be updated
  //     [
  //       "0.0026",       // Price level to be updated
  //       "100"           // Quantity
  //     ]
  //   ]
  // }
  j.at("E").get_to(obu.timestamp);
  j.at("U").get_to(obu.first_update_id);
  j.at("u").get_to(obu.last_update_id);
  j.at("b").get_to(obu.bids);
  j.at("a").get_to(obu.asks);
}

[[nodiscard]] std::string OrderBookHandler::stream_name() const noexcept {
  return "depth@100ms";
}

void OrderBookHandler::handle(const nlohmann::json& data) const {
  try {
    const OrderBookUpdate update = data.get<OrderBookUpdate>();
    std::cout << "Order book update: " << update.first_update_id << " -> "
              << update.last_update_id << "(bids: " << update.bids.size()
              << ", asks: " << update.asks.size() << ")\n";
  } catch (const std::exception& e) {
    std::cerr << "JSON parse error: " << e.what() << '\n' << data << '\n';
  }
}
