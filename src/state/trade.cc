module;
#include "boost/asio/awaitable.hpp"
#include "nlohmann/json.hpp"
#include "spdlog/spdlog.h"

module state;

namespace state {
inline void from_json(const nlohmann::json& j, Trade& t) {
  // {
  //   "e": "aggTrade",    // Event type
  //   "E": 1672515782136, // Event time
  //   "s": "BNBBTC",      // Symbol
  //   "a": 12345,         // Aggregate trade ID
  //   "p": "0.001",       // Price
  //   "q": "100",         // Quantity
  //   "f": 100,           // First trade ID
  //   "l": 105,           // Last trade ID
  //   "T": 1672515782136, // Trade time
  //   "m": true,          // Is the buyer the market maker?
  //   "M": true           // Ignore
  // }
  using namespace std::chrono;
  t.event_time = sys_time{milliseconds{j.at("E").get<uint64_t>()}};
  j.at("s").get_to(t.symbol);
  j.at("a").get_to(t.trade_id);
  t.price = std::stod(j.at("p").get<std::string>());
  t.quantity = std::stod(j.at("q").get<std::string>());
  j.at("f").get_to(t.first_trade_id);
  j.at("l").get_to(t.last_trade_id);
  t.trade_time = sys_time{milliseconds{j.at("T").get<uint64_t>()}};
  j.at("m").get_to(t.is_buyer_market_maker);
}

boost::asio::awaitable<void> TradeHandler::handle(const nlohmann::json& data) {
  Trade trade{};
  try {
    trade = data.get<Trade>();
  } catch (const std::exception& e) {
    spdlog::error("JSON parse error: {} (`{}`)", e.what(), data.dump());
    co_return;
  }

  subject_.get_observer().on_next(trade);
  co_return;
}
}  // namespace state
