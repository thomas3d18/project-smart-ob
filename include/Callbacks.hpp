#pragma once
#include "DataStructures.hpp"
#include "Types.hpp"

class OrderBook;

using OrderBookCallback = std::function<void(const OrderBook&, const OrderInfo&)>;

struct Callbacks {
    OrderBookCallback onOrderAdd;
    OrderBookCallback onOrderCancel;
    OrderBookCallback onOrderModify;
    OrderBookCallback onOrderExecution;
};

void onOrderAdd(OrderBook& smartOrderBook, const OrderInfo& orderInfo);
void onOrderCancel(OrderBook& smartOrderBook, const OrderInfo& orderInfo);
void onOrderModify(OrderBook& smartOrderBook, const OrderInfo& orderInfo);
void onOrderExecution(OrderBook& smartOrderBook, const OrderInfo& orderInfo);