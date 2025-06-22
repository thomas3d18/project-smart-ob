#pragma once
#include "Types.hpp"
#include "DataStructures.hpp"
#include <iostream>

struct L3PriceLevel {
    Price price;
    Quantity quantity;
    int numOrders;
    std::list<Order> orders;

    L3PriceLevel(Price p = 0.0) : price(p), quantity(0), numOrders(0) {}
};

class L3Book {
private:
    OneSideBook<L3PriceLevel, BidComparator> bidBook;
    OneSideBook<L3PriceLevel, AskComparator> askBook;
    std::unordered_map<int, std::list<Order>::iterator> orderMap;

    template<typename BookType>
    void removeOrderFromLevel(BookType& book, Price price, std::list<Order>::iterator orderIt) {
        auto levelIt = book.find(price);

        if (levelIt != book.end()) {
            L3PriceLevel& level = levelIt->second;
            level.quantity -= orderIt->size;
            level.numOrders--;
            level.orders.erase(orderIt);

            if (level.numOrders == 0) {
                std::cout << "Remove price level " << price << std::endl;
                book.erase(levelIt);
            }
        }
    }

public:
    std::string name;

    L3Book() : name("L3Book") {}

    bool addOrder(OrderId orderId, bool isSell, Quantity size, Price price);
    bool cancelOrder(OrderId orderId);
    bool cancelOrder(Order& order);
    bool modifyOrder(OrderId orderId, Quantity newSize, Price newPrice);
    bool modifyOrderSize(Order& order, int newSize);
    bool modifyOrderId(OrderId orderId, OrderId newId);
    bool executeOrder(Order& order, Quantity executedSize);
    std::vector<OrderInfo> executeAtPrice(Price price, Quantity quantity, bool isGuess);

    bool hasOrder(OrderId orderId) const;
    Order* findOrder(OrderId orderId) const;
    const OneSideBook<L3PriceLevel, BidComparator>& getBids() const { return bidBook; }
    const OneSideBook<L3PriceLevel, AskComparator>& getAsks() const { return askBook; }
    std::vector<L3PriceLevel> getTopAsks(int n=5) const;
    std::vector<L3PriceLevel> getTopBids(int n=5) const;

    Price getBestBid() const;
    Price getBestAsk() const;
    bool isOrderBookCrossed() const;
    bool empty() const { return bidBook.empty() && askBook.empty(); }

    void clear();
    size_t getTotalOrders() const { return orderMap.size(); }

    void printBook(int levels=5) const;
};