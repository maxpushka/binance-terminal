module;
#include "boost/asio/awaitable.hpp"
#include "nlohmann/json.hpp"
#include "spdlog/spdlog.h"

module state;

namespace state {
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
  j.at("s").get_to(obu.symbol);
  j.at("E").get_to(obu.timestamp);
  j.at("U").get_to(obu.first_update_id);
  j.at("u").get_to(obu.last_update_id);
  j.at("b").get_to(obu.bids);
  j.at("a").get_to(obu.asks);
}

void OrderBookHandler::apply_update(const OrderBookUpdate& update) {
  // Process bids
  for (const auto& bid : update.bids) {
    const double price = std::stod(bid[0]);
    if (const double qty = std::stod(bid[1]); qty == 0.0) {
      order_book_.bids.erase(price);
    } else {
      order_book_.bids[price] = OrderBookEntry{price, qty};
    }
  }
  // Process asks
  for (const auto& ask : update.asks) {
    const double price = std::stod(ask[0]);
    if (const double qty = std::stod(ask[1]); qty == 0.0) {
      order_book_.asks.erase(price);
    } else {
      order_book_.asks[price] = OrderBookEntry{price, qty};
    }
  }
  // Update the current update id to that of the processed event.
  current_update_id_ = update.last_update_id;
}

boost::asio::awaitable<void> OrderBookHandler::handle(
    const nlohmann::json& data) {
  OrderBookUpdate update{};
  try {
    update = data.get<OrderBookUpdate>();
  } catch (const std::exception& e) {
    spdlog::error("JSON parse error: {} (`{}`)", e.what(), data.dump());
    co_return;
  }

  spdlog::debug(
      "Order book update received: "
      "first_update_id={} last_update_id={} "
      "(bids: {}, asks: {})",
      update.first_update_id, update.last_update_id, update.bids.size(),
      update.asks.size());

  if (!initialized_) {
    // Buffer the update until the snapshot is applied.
    buffered_updates_.push_back(update);

    if (!snapshot_requested_) {
      // Record the U from the first event.
      first_event_U_ = update.first_update_id;
      snapshot_requested_ = true;

      // Fetch a full snapshot.
      auto snapshot = co_await api_.get_orderbook_snapshot(update.symbol);
      // Ensure the snapshot's update id is >= the first event's U.
      while (snapshot.last_update_id < first_event_U_) {
        spdlog::warn(
            "Snapshot last_update_id ({}) is less than first event U ({}). "
            "Refetching snapshot...",
            snapshot.last_update_id, first_event_U_);
        snapshot = co_await api_.get_orderbook_snapshot(update.symbol);
      }

      // Initialize the local order book with the snapshot.
      order_book_.bids.clear();
      order_book_.asks.clear();
      for (const auto& bid : snapshot.bids) {
        const double price = std::stod(bid[0]);
        const double qty = std::stod(bid[1]);
        if (qty == 0.0) continue;
        order_book_.bids[price] = OrderBookEntry{price, qty};
      }
      for (const auto& ask : snapshot.asks) {
        const double price = std::stod(ask[0]);
        const double qty = std::stod(ask[1]);
        if (qty == 0.0) continue;
        order_book_.asks[price] = OrderBookEntry{price, qty};
      }
      current_update_id_ = snapshot.last_update_id;
      spdlog::info("Order book snapshot applied with last_update_id={}",
                   current_update_id_);

      // In the buffered events, discard any event where u <=
      // snapshot.last_update_id.
      while (!buffered_updates_.empty() &&
             buffered_updates_.front().last_update_id <= current_update_id_) {
        buffered_updates_.pop_front();
      }

      // Process the remaining buffered events.
      for (const auto& buffered_update : buffered_updates_) {
        // Ignore any update that is already stale.
        if (buffered_update.last_update_id < current_update_id_) continue;
        // If the update’s first update id is greater than our current update
        // id, there is a gap: restart the sync process.
        if (buffered_update.first_update_id > current_update_id_) {
          spdlog::error(
              "Gap in buffered updates detected: first_update_id {} > "
              "current_update_id {}. Restarting sync.",
              buffered_update.first_update_id, current_update_id_);
          // Reset the state.
          initialized_ = false;
          buffered_updates_.clear();
          order_book_.bids.clear();
          order_book_.asks.clear();
          snapshot_requested_ = false;
          co_return;
        }
        apply_update(buffered_update);
        spdlog::debug("Applied buffered update: new current_update_id={}",
                      current_update_id_);
      }
      buffered_updates_.clear();
      initialized_ = true;
    }
    co_return;
  }

  // Already initialized – process the update directly.
  if (update.last_update_id < current_update_id_) {
    spdlog::warn(
        "Stale update ignored: update.last_update_id ({}) < "
        "current_update_id ({}).",
        update.last_update_id, current_update_id_);
    co_return;
  }
  if (update.first_update_id > current_update_id_) {
    spdlog::error(
        "Missing updates: update.first_update_id ({}) > current_update_id "
        "({}). Restarting sync.",
        update.first_update_id, current_update_id_);
    // Reset state for a full re-sync.
    initialized_ = false;
    buffered_updates_.clear();
    order_book_.bids.clear();
    order_book_.asks.clear();
    snapshot_requested_ = false;
    co_return;
  }
  apply_update(update);
  spdlog::debug("Applied update: new current_update_id={}", current_update_id_);

  // Publish the update so that other components can use it.
  subject_.get_observer().on_next(order_book_);
  co_return;
}
}  // namespace state
