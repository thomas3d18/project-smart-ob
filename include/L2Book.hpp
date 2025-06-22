#pragma once
#include "Types.hpp"
#include "DataStructures.hpp"

struct L2PriceLevel {
    Price price;
    Quantity quantity;

    L2PriceLevel(Price p = 0.0, Quantity q = 0) : price(p), quantity(q) {}
};

class L2Book {
private:
    OneSideBook<L2PriceLevel, BidComparator> bidBook;
    OneSideBook<L2PriceLevel, AskComparator> askBook;
    Timestamp lastUpdateTime;

public:
    void addBidLevel(Price price, Quantity quantity);
    void addAskLevel(Price price, Quantity quantity);
    void clear();
    const OneSideBook<L2PriceLevel, BidComparator>& getBids() { return bidBook; }
    const OneSideBook<L2PriceLevel, AskComparator>& getAsks() { return askBook; }

    Price getBestBid() const;
    Price getBestAsk() const;
    Quantity getBidQuantityAtPrice(Price price) const;
    Quantity getAskQuantityAtPrice(Price price) const;

    bool isEmpty() const { return bidBook.empty() && askBook.empty(); }
    Timestamp getLastUpdateTime() const { return lastUpdateTime; }
};