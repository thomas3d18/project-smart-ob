#pragma once
#include "L2Book.hpp"
#include "L3Book.hpp"
#include "TradeContainer.hpp"
#include "Callbacks.hpp"
#include <unordered_map>
#include <functional>
#include <queue>
#include <string>
#include <random>

class OrderBook {
private:
    L3Book smartBook;
    L2Book* l2Book;
    L3Book* l3Book;
    TradeContainer* tradeContainer;

    Callbacks callbacks;

    // deduction logic
    std::unordered_map<OrderId, OrderInfo> guesses;
    std::list<OrderInfo> aggressors;
    std::queue<OrderInfo*> guessedExecutions;
    Timestamp lastReconciliationTime;

    // random variables
    double executionProbability = 0.3;
    std::mt19937 rngEngine;
    std::uniform_real_distribution<> dist;

public:
    OrderBook() {};
    OrderBook(L2Book& l2Book, L3Book& l3Book, TradeContainer& trades, double executionProbability=0.3);

    void setCallbacks(const Callbacks& callbackset) { callbacks = callbackset; }

    // process market data
    void processL2Snapshot(const std::string& data, Timestamp timestamp);
    void processL3Update(const std::string& data, Timestamp timestamp);
    void processTrade(const TradeInfo& trade);
    void processTrade(const std::string& data, Timestamp timestamp);

    // smart deduction methods
    std::pair<bool, bool> deduceIsSellAggressor(Price price) const;
    void handleL2BidChange(Price price, Timestamp);
    void handleL2AskChange(Price price, Timestamp);
    void guessNewOrder(Price price, Quantity quantity, bool isSell, bool isMarketable, Timestamp timestamp, bool isGuess=false);
    void guessOrderReduction(Price price, Quantity quantity, bool isSell, Timestamp timestamp);
    void onExecution(Price price, Quantity quantity, Timestamp timestamp, bool isGuess);
    bool reconcileAdd(OrderId orderId, bool isSell, Price price, Quantity size);
    bool reconcileModify(OrderId orderId, Price price, Quantity size);
    bool reconcileCancel(OrderId orderId);
    bool reconcileTrade(Price price, Quantity quantity);

    std::unordered_map<OrderId, OrderInfo> getGuesses() const { return guesses; }
    std::list<OrderInfo> getAggressors() const { return aggressors; }

    const L3Book& getSmartOrderBook() { return smartBook; }

    void onOrderAdd(OrderBook& smartOrderBook, const OrderInfo& orderInfo);
    void onOrderCancel(OrderBook& smartOrderBook, const OrderInfo& orderInfo);
    void onOrderModify(OrderBook& smartOrderBook, const OrderInfo& orderInfo);
    void onOrderExecution(OrderBook& smartOrderBook, const OrderInfo& orderInfo);

};