#include "orderbook.h"

#include <iostream>

#include "nlohmann/json.hpp"

struct OrderBookSnapshot {
  int64_t last_update_id;
  std::vector<std::vector<std::string>> bids;
  std::vector<std::vector<std::string>> asks;
};

void from_json(const nlohmann::json& j, OrderBookSnapshot& obs) {
  j.at("lastUpdateId").get_to(obs.last_update_id);
  obs.bids = j.at("bids").get<decltype(OrderBookSnapshot::bids)>();
  obs.asks = j.at("asks").get<decltype(OrderBookSnapshot::asks)>();
}

struct OrderBookUpdate {
  int64_t first_update_id;
  int64_t last_update_id;
  std::vector<std::vector<std::string>> bids;
  std::vector<std::vector<std::string>> asks;
};

void from_json(const nlohmann::json& j, OrderBookUpdate& obu) {
  j.at("U").get_to(obu.first_update_id);
  j.at("u").get_to(obu.last_update_id);
  j.at("b").get_to(obu.bids);
  j.at("a").get_to(obu.asks);
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
