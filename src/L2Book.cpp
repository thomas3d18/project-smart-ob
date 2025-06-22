#include "L2Book.hpp"

void L2Book::addAskLevel(Price price, Quantity quantity) {
    askBook[price] = {price, quantity};
}

void L2Book::addBidLevel(Price price, Quantity quantity) {
    bidBook[price] = {price, quantity};
}

Price L2Book::getBestBid() const {
    return bidBook.empty() ? 0.0 : bidBook.begin()->first;
}

Price L2Book::getBestAsk() const {
    return askBook.empty() ? 0.0 : askBook.begin()->first;
}

Quantity L2Book::getBidQuantityAtPrice(Price price) const {
    auto it = bidBook.find(price);
    if (it != bidBook.end()) return it->second.quantity;
    return -1;
}

Quantity L2Book::getAskQuantityAtPrice(Price price) const {
    auto it = askBook.find(price);
    if (it != askBook.end()) return it->second.quantity;
    return -1;
}

void L2Book::clear() {
    bidBook.clear();
    askBook.clear();
}