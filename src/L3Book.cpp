#include "L3Book.hpp"
#include <iostream>

Price L3Book::getBestAsk() const {
    return askBook.begin()->first;
}

Price L3Book::getBestBid() const {
    return bidBook.begin()->first;
}

Order* L3Book::findOrder(OrderId orderId) const {
    auto it = orderMap.find(orderId);
    if (it != orderMap.end()) {
        return const_cast<Order*>(&(*it->second));
    }
    return nullptr;
}

bool L3Book::hasOrder(OrderId orderId) const {
    return orderMap.find(orderId) != orderMap.end();
}

bool L3Book::addOrder(OrderId orderId, bool isSell, Quantity size, Price price) {
    // order id already exists
    if (orderMap.find(orderId) != orderMap.end()) {
        return false;
    }

    if (price <= 0.0) {
        std::cout << "Not adding Mkt order\n";
        return false;
    }

    std::cout << "Adding order " << orderId << "\n";
    Order order(orderId, isSell, price, size);
    std::list<Order>::iterator orderIt;

    auto& level = (isSell ? askBook[price] : bidBook[price]);

    if (level.numOrders == 0) {
        level.price = price;
    }
    level.orders.push_back(order);
    orderIt = std::prev(level.orders.end());
    level.quantity += order.size;
    level.numOrders++;
    orderMap[order.orderId] = orderIt;

    // if (order.isSell) {
    //     auto& level = askBook[price];
    //     if (level.numOrders == 0) {
    //         level.price = price;
    //     }
    //     level.orders.push_back(order);
    //     orderIt = std::prev(level.orders.end());
    //     level.quantity += order.size;
    //     level.numOrders++;
    // } else {
    //     auto& level = bidBook[price];
    //     if (level.numOrders == 0) {
    //         level.price = price;
    //     }
    //     level.orders.push_back(order);
    //     orderIt = std::prev(level.orders.end());
    //     level.quantity += order.size;
    //     level.numOrders++;
    // }

    orderMap[order.orderId] = orderIt;
    return true;
}

bool L3Book::cancelOrder(OrderId orderId) {
    auto mapIt = orderMap.find(orderId);
    // order not found
    if (mapIt == orderMap.end()) {
        return false;
    }

    std::cout << "Cancelling order id " << orderId << "\n";

    auto orderIt = mapIt->second;
    Price price = orderIt->price;
    bool isSell = orderIt->isSell;
    auto levelIt = (isSell ? askBook.find(price) : bidBook.find(price));
    auto end = (isSell ? askBook.end() : bidBook.end());

    if (levelIt != end) {
        L3PriceLevel& level = levelIt->second;
        level.quantity -= orderIt->size;
        level.numOrders--;
        level.orders.erase(orderIt);



        if (level.numOrders == 0) {
            std::cout << "Remove price level " << price << std::endl;
            if (isSell) {
                askBook.erase(levelIt);
            } else {
                bidBook.erase(levelIt);
            }
        }
    }

    // if (orderIt->isSell) {
    //     removeOrderFromLevel(askBook, price, orderIt);
    // } else {
    //     removeOrderFromLevel(bidBook, price, orderIt);
    // }

    orderMap.erase(mapIt);
    //printBook();
    return true;
}

bool L3Book::modifyOrderSize(Order& order, int newSize) {
    if (newSize <= 0) {
        return false;
    }
    int sizeDelta = order.size - newSize;
    order.size = newSize;
    if (order.isSell) {
        auto levelIt = askBook.find(order.price);
        if (levelIt != askBook.end()) {
            levelIt->second.quantity -= sizeDelta;
        }
    } else {
        auto levelIt = bidBook.find(order.price);
        if (levelIt != bidBook.end()) {
            levelIt->second.quantity -= sizeDelta;
        }
    }
    return true;
}

bool L3Book::modifyOrderId(OrderId orderId, OrderId newId) {
    auto mapIt = orderMap.find(orderId);
    if (mapIt == orderMap.end()) {
        std::cerr << "Order not found\n";
        return false;
    }
    mapIt->second->orderId = newId;
    return true;
}

bool L3Book::modifyOrder(OrderId orderId, Quantity newSize, Price newPrice) {
    //std::cout << "modifying order id " << orderId << " size " << newSize << " price " << newPrice << "\n";
    auto mapIt = orderMap.find(orderId);
    if (mapIt == orderMap.end()) {
        std::cerr << "Order not found\n";
        return false;
    }

    // replace
    auto orderIt = mapIt->second;

    // amend down
    if (orderIt->price == newPrice && orderIt->size > newSize) {
        std::cout << "[" << name << "] Amending down order " << orderId << "\n";
        return modifyOrderSize(*orderIt, newSize);
    }

    std::cout << "[" << name << "] Replacing order " << orderId << "\n";
    Order oldOrder = *orderIt;
    cancelOrder(orderId);
    //Order newOrder(orderId, oldOrder.side, newPrice, newSize);
    return addOrder(orderId, oldOrder.isSell, newSize, newPrice);
}

bool L3Book::executeOrder(Order& order, Quantity executedSize) {
    // auto mapIt = orderMap.find(orderId);
    // if (mapIt == orderMap.end()) {
    //     return false;
    // }

    //auto orderIt = mapIt->second;
    if (executedSize <= 0) {
        return false;
    }

    // fully filled
    if (order.size == executedSize) {
        return cancelOrder(order.orderId);
    }

    // partial fill
    order.size -= executedSize;

    if (order.isSell) {
        auto levelIt = askBook.find(order.price);
        if (levelIt != askBook.end()) {
            levelIt->second.quantity -= executedSize;
        }
    } else {
        auto levelIt = bidBook.find(order.price);
        if (levelIt != bidBook.end()) {
            levelIt->second.quantity -= executedSize;
        }
    }

    return true;
}

std::vector<OrderInfo> L3Book::executeAtPrice(Price price, Quantity quantity, bool isGuess) {
    if (price != getBestAsk() && price != getBestBid()) {
        std::cerr << "[CRITICAL] Unable to find price level " << price << " for trade\n";
    }

    bool isAsk = price == getBestAsk();
    auto levelIt = (isAsk ? askBook.find(price) : bidBook.find(price));
    auto end = (isAsk ? askBook.end() : bidBook.end());
    std::vector<OrderInfo> executions;
    if (levelIt == end) {
        std::cerr << "[CRITICAL] Unable to find price level " << price << " for trade\n";
        return executions;
    }

    Quantity remainingQty = quantity;
    auto& level = levelIt->second;
    auto orderIt = level.orders.begin();
    while (orderIt != level.orders.end() && remainingQty > 0) {
        auto currIt = orderIt++;
        int execQty = std::min(remainingQty, currIt->size);
        OrderInfo execution(currIt->orderId, currIt->isSell, price, execQty, "EXECUTION");
        execution.originalQty = currIt->size;
        if (isGuess) {
            execution.isGuess = true;
            execution.isPending = true;
        }
        executions.push_back(execution);
        executeOrder(*currIt, execQty);
        remainingQty -= execQty;
    }

    return executions;

    // if (price == getBestAsk()) {
    //     return executeAtPrice(askBook, price, quantity);
    // } else if (price == getBestBid()) {
    //     return executeAtPrice(bidBook, price, quantity);
    // }
    // return {};
}

bool L3Book::isOrderBookCrossed() const {
    if (bidBook.empty() || askBook.empty()) {
        return false;
    }

    return bidBook.begin()->first >= askBook.begin()->first;
}

void L3Book::clear() {
    bidBook.clear();
    askBook.clear();
    orderMap.clear();
}

void L3Book::printBook(int levels) const {
    std::cout << "[" << name << "] total # of orders: " << getTotalOrders() << "\n";

    std::cout << "-Bids:" << "\n";
    for (const auto& [price, level] : bidBook) {
        std::cout << "--Price: " << price << " qty: " << level.quantity << " orders: " << level.numOrders << "\n";
        for (const auto& order : level.orders) {
            std::cout << "---[Id: " << order.orderId << " " << (order.isSell ? "Sell " : "Buy ") << order.size << "]\n";
        }
    }

    std::cout << "-Asks:" << "\n";
    for (const auto& [price, level] : askBook) {
        std::cout << "--Price: " << price << " qty: " << level.quantity << " orders: " << level.numOrders << "\n";
        for (const auto& order : level.orders) {
            std::cout << "---[Id: " << order.orderId << " " << (order.isSell ? "Sell " : "Buy ") << order.size << "]\n";
        }
    }
}

std::vector<L3PriceLevel> L3Book::getTopAsks(int n) const {
    std::vector<L3PriceLevel> res;
    res.reserve(n);

    int count = 0;
    for (const auto& pair : askBook) {
        if (count >= n) break;
        res.push_back(pair.second);
        count++;
    }

    return res;
}

std::vector<L3PriceLevel> L3Book::getTopBids(int n) const {
    std::vector<L3PriceLevel> res;
    res.reserve(n);

    int count = 0;
    for (const auto& pair : bidBook) {
        if (count >= n) break;
        res.push_back(pair.second);
        count++;
    }

    return res;
}
