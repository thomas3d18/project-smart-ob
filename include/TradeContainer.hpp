#pragma once
#include "Types.hpp"
#include <vector>

class TradeContainer {
private:
    std::vector<TradeInfo> trades;
    size_t maxSize;

public:
    explicit TradeContainer(size_t maxTrades = 10000) : maxSize(maxTrades) {
        trades.reserve(maxSize);
    }

    void addTrade(const TradeInfo& trade);
    const std::vector<TradeInfo>& getTrades() const { return trades; }

    std::vector<TradeInfo> getTradesAfter(Timestamp timestamp) const;
    TradeInfo getLastTrade() const;

    void clear() { trades.clear(); }
    bool empty() const { return trades.empty(); }
};