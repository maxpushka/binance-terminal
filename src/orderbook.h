#pragma once

#include "websocket_base.h"

/*
```go
// binanceDepthSnapshot represents a snapshot of the order book
// that is obtained from the REST API, not the WebSocket.
type binanceDepthSnapshot struct {
	LastUpdateID int64      `json:"lastUpdateId"`
	Bids         [][]string `json:"bids"`
	Asks         [][]string `json:"asks"`
}

// binanceDepthEvent represents an incremental update to
// the order book that is obtained from the WebSocket.
type binanceDepthEvent struct {
	FirstUpdateID int64      `json:"U"`
	LastUpdateID  int64      `json:"u"`
	Bids          [][]string `json:"b"`
	Asks          [][]string `json:"a"`
}
```
*/
struct OrderBookSnapshot {
  int64_t last_update_id;
  std::vector<std::vector<std::string>> bids;
  std::vector<std::vector<std::string>> asks;
};

struct OrderBookUpdate {
  int64_t first_update_id;
  int64_t last_update_id;
  std::vector<std::vector<std::string>> bids;
  std::vector<std::vector<std::string>> asks;
};

class BinanceOrderBookWebSocketClient : public BinanceWebSocketBase {
 public:
  // Inherit the constructor.
  using BinanceWebSocketBase::BinanceWebSocketBase;

 protected:
  // Specify the handshake target for order book streams.
  std::vector<std::string> get_subscription_streams() const override;
  // Process an order book message.
  void process_message(const std::string& message) override;
};
