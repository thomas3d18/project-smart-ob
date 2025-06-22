#pragma once
#include <string>

using Timestamp = uint64_t;
using Price = double;
using Quantity = int;
using OrderId = int;

enum class EventType {
    L2_SNAPSHOT,
    L3_UPDATE,
    TRADE_EXECUTION
};

enum class OrderSide {
    BUY,
    SELL
};

struct Order {
    OrderId orderId;
    bool isSell;
    Price price;
    Quantity size;
    Timestamp Timestamp;

    Order() {}
    Order(OrderId orderId, bool isSell, Price price, Quantity size)
        : orderId(orderId), isSell(isSell), price(price), size(size) {}

};

struct OrderInfo {
    OrderId orderId;
    bool isSell;
    Price price;
    Quantity size;
    std::string action;
    Timestamp timestamp;
    Quantity originalQty;
    bool isGuess;
    bool isMarketable;
    bool isPending = false;

    OrderInfo(OrderId orderId, 
                bool isSell, 
                Price price, 
                Quantity size, 
                std::string action, 
                Timestamp timestamp=0,
                Quantity originalQty=0,
                bool isGuess=false,
                bool isMarketable=false)
        : orderId(orderId), 
        isSell(isSell), 
        price(price), 
        size(size), 
        action(action), 
        timestamp(timestamp),
        originalQty(originalQty),
        isGuess(isGuess),
        isMarketable(isMarketable)
        {}
};

struct TradeInfo {
    Price price;
    Quantity quantity;
    Timestamp timestamp;
    OrderSide aggressorSide;
    OrderId orderId;
};

struct MarketEvent {
    EventType type;
    Timestamp timestamp;
    std::string rawData;
};

struct PendingAction {
    enum Type { ADD, CANCEL, MODIFY, EXECUTE } type;
    Order order;
    Timestamp expectedTime;
    double confidence;

    PendingAction(Type t, const Order& order, Timestamp time, double conf=1.0)
        : type(t), order(order), expectedTime(time), confidence(conf) {}
};