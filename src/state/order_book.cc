module;
#include "nlohmann/json.hpp"
#include "spdlog/spdlog.h"

module state;

import exchange;

inline void from_json(const nlohmann::json& j, OrderBookUpdate& obu) {
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
  OrderBookUpdate update{};
  try {
    update = data.get<OrderBookUpdate>();
  } catch (const std::exception& e) {
    spdlog::error("JSON parse error: {} (`{}`)", e.what(), data.dump());
    return;
  }

  spdlog::debug("order book update: {} -> {} (bids: {}, asks: {})",
                update.first_update_id, update.last_update_id,
                update.bids.size(), update.asks.size());
}
