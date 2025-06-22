#include "OrderBook.hpp"
#include <iostream>
#include <sstream>

OrderBook::OrderBook(L2Book& l2Book, L3Book& l3Book, TradeContainer& trades, double executionProbability)
    : l2Book(&l2Book), l3Book(&l3Book), tradeContainer(&trades), lastReconciliationTime(0), 
        executionProbability(executionProbability), dist(0.0, 1.0) {
        if (l3Book.getBestBid() > 0 || l3Book.getBestAsk() > 0) {
            smartBook = l3Book;
        }
        smartBook.name = "SmartBook";
        std::mt19937 rngEngine;
        std::uniform_real_distribution<> dist;
    }

void OrderBook::processL2Snapshot(const std::string& data, Timestamp timestamp) {
    std::istringstream ss(data);
    std::string token;
    Price price;
    Quantity qty;

    l2Book->clear();

    // should start with BID
    ss >> token;
    while (ss >> token && token != "ASK") {
        Price price = std::stod(token);
        ss >> qty;
        l2Book->addBidLevel(price, qty);
    }

    while (ss >> token) {
        Price price = std::stod(token);
        ss >> qty;
        l2Book->addAskLevel(price, qty);
    }

    handleL2BidChange(price, timestamp);
    handleL2AskChange(price, timestamp);
}

void OrderBook::handleL2BidChange(Price price, Timestamp timestamp) {
    auto& l3Bids = smartBook.getBids();
    for (const auto& [price, l2Level] : l2Book->getBids()) {

        auto it = l3Bids.find(price);
        if (it == l3Bids.end()) {
            std::cout << "[L3] New price level found: " << price << "\n";
            guessNewOrder(price, l2Level.quantity, false, false, timestamp);
        } else if (l2Level.quantity > it->second.quantity) {
            guessNewOrder(price, l2Level.quantity - it->second.quantity, false, false, timestamp, true);
        } else if (it->second.quantity > l2Level.quantity){
            guessOrderReduction(price, it->second.quantity - l2Level.quantity, false, timestamp);
        }
    }

    // check for removed price level in L3
    for (auto it = l3Bids.begin(); it != l3Bids.end(); ) {
        Price price = it->first;
        auto currIt = it++;
        const auto& book = l2Book->getBids();
        if (book.find(price) == book.end()) {
            std::cout << "Reducing price level " << price << "\n";
            guessOrderReduction(price, currIt->second.quantity, false, timestamp);
        }
    }
}

void OrderBook::handleL2AskChange(Price price, Timestamp timestamp) {
    auto& l3Asks = smartBook.getAsks();
    for (const auto& [price, l2Level] : l2Book->getAsks()) {

        auto it = l3Asks.find(price);
        if (it == l3Asks.end()) {
            std::cout << "[L3] New price level found: " << price << "\n";
            guessNewOrder(price, l2Level.quantity, true, false, timestamp);
        } else if (l2Level.quantity > it->second.quantity) {
            guessNewOrder(price, l2Level.quantity - it->second.quantity, true, false, timestamp, true);
        } else if (it->second.quantity > l2Level.quantity){
            guessOrderReduction(price, it->second.quantity - l2Level.quantity, true, timestamp);
        }
    }

    // check for removed price level in L3
    for (auto it = l3Asks.begin(); it != l3Asks.end(); ) {
        Price price = it->first;
        auto currIt = it++;
        const auto& book = l2Book->getAsks();
        if (book.find(price) == book.end()) {
            std::cout << "Reducing price level " << price << "\n";
            guessOrderReduction(price, currIt->second.quantity, true, timestamp);
        }
    }
}

void OrderBook::guessOrderReduction(Price price, Quantity quantity, bool isSell, Timestamp timestamp) {
    auto levelIt = (isSell ? smartBook.getAsks().find(price) : smartBook.getBids().find(price));
    auto bookEnd = (isSell ? smartBook.getAsks().end() : smartBook.getBids().end());
    if (levelIt == bookEnd) {
        return;
    }

    Quantity remainingQty = quantity;
    auto& level = levelIt->second;
    bool isCancelLevel = level.quantity == quantity;
    auto orderIt = level.orders.begin();
    while (orderIt != level.orders.end() && remainingQty > 0) {
        auto currIt = orderIt++;
        int reduceQty = std::min(remainingQty, currIt->size);
        double rand = dist(rngEngine);
        if (rand < executionProbability) {
            onExecution(price, reduceQty, timestamp, true);
        } else {
            if (reduceQty == currIt->size) {
                OrderInfo info(currIt->orderId, currIt->isSell, currIt->price, reduceQty, "CANCEL", timestamp);
                info.isGuess = true;
                guesses.insert({currIt->orderId, info});
                smartBook.cancelOrder(currIt->orderId);
                onOrderCancel(*this, info);
            } else {
                double newSize = currIt->size - reduceQty;
                OrderInfo info(currIt->orderId, currIt->isSell, currIt->price, newSize, "MODIFY", timestamp);
                info.originalQty = currIt->size;
                info.isGuess = true;
                guesses.insert({currIt->orderId, info});
                smartBook.modifyOrder(currIt->orderId, newSize, currIt->price);
                onOrderModify(*this, info);
            }
        }
        remainingQty -= reduceQty;
    }
}

void OrderBook::processTrade(const std::string& data, Timestamp timestamp) {
    std::istringstream ss(data);
    TradeInfo trade;

    trade.timestamp = timestamp;
    ss >> trade.price >> trade.quantity;

    tradeContainer->addTrade(trade);
    std::cout << "-[TOTAL TRADES] " << tradeContainer->getTrades().size() << "\n";

    if (!reconcileTrade(trade.price, trade.quantity))
    {
        onExecution(trade.price, trade.quantity, timestamp, false);
    }
}

void OrderBook::processL3Update(const std::string& data, Timestamp timestamp) {
    std::istringstream ss(data);
    std::string action, side;
    Price price;
    Quantity size;
    int orderId;

    ss >> action >> orderId >> side >> price >> size;
    bool isSell = (side == "SELL" ? true : false);
    OrderSide orderSide = (side == "SELL") ? OrderSide::SELL : OrderSide::BUY;

    if (action == "ADD") {
        l3Book->addOrder(orderId, isSell, size, price);

        // check if it is a previous aggressor
        if (!reconcileAdd(orderId, isSell, price, size)) {
            smartBook.addOrder(orderId, isSell, size, price);
            onOrderAdd(*this, OrderInfo(orderId, isSell, price, size, "ADD", timestamp));
        }
    } else if (action == "MODIFY") {
        l3Book->modifyOrder(orderId, size, price);

        if (!reconcileModify(orderId, price, size)) {
            smartBook.modifyOrder(orderId, size, price);
            onOrderModify(*this, OrderInfo(orderId, isSell, price, size, "MODIFY", timestamp));
        }
    } else if (action == "CANCEL") {
        l3Book->cancelOrder(orderId);

        if (!reconcileCancel(orderId)) {
            smartBook.cancelOrder(orderId);
            onOrderCancel(*this, OrderInfo(orderId, isSell, price, size, "CANCEL", timestamp));
        }
    }

    l3Book->printBook();
    smartBook.printBook();
}

bool OrderBook::reconcileAdd(OrderId orderId, bool isSell, Price price, Quantity size) {
    for (auto it = aggressors.begin(); it != aggressors.end(); ) {
        if (it->isMarketable && it->isSell == isSell && it->price == price && it->size == size) {
            // we have received ADD for the aggressor, expect the CANCEL to be received before removing
            it->isPending = true;
            it->orderId = orderId;
            guesses.insert({orderId, *it});
            aggressors.erase(it);
            return true;
        }
        ++it;
    }

    for (auto it = guesses.begin(); it != guesses.end(); ) {
        OrderInfo& guess = it->second;
        if (guess.action == "ADD" && guess.isSell == isSell && guess.price == price && guess.size == size) {
            if (guess.orderId < 0)
                smartBook.modifyOrderId(guess.orderId, orderId);
            guesses.erase(it);
            return true;
        }
        ++it;
    }
    return false;
}

bool OrderBook::reconcileModify(OrderId orderId, Price price, Quantity size) {
    auto it = guesses.find(orderId);
    if (it != guesses.end()) {
        // check if we have already applied partial fill from guessing
        if (it->second.action == "EXECUTION" &&
            it->second.price == price && it->second.originalQty - it->second.size == size) {
            it->second.isPending = false;
            if (!it->second.isGuess)    
                guesses.erase(it);
            return true;
        }

        if (it->second.action == "MODIFY" && it->second.isGuess) {
            return true;
        }
    }

    for (auto it = guesses.begin(); it != guesses.end(); ) {
        OrderInfo& guess = it->second;
        std::cout << "FOUND guess " << guess.orderId << " " << guess.price << " " << guess.size << " " << guess.isGuess << std::endl;
        if (guess.action == "ADD" && guess.price == price) {
            // invalidate new order guess, apply amend instead
            if (guess.isGuess && smartBook.hasOrder(orderId))
            {
                smartBook.cancelOrder(guess.orderId);
                guesses.erase(it);
                return false;
            }
            if (guess.size == size) {
                // update real order id of new order
                if (guess.orderId < 0)
                    smartBook.modifyOrderId(guess.orderId, orderId);
                guesses.erase(it);
                return true;
            }
        }
        ++it;
    }
    return false;
}

bool OrderBook::reconcileCancel(OrderId orderId) {
    auto it = guesses.find(orderId);
    if (it != guesses.end()) {
        if (it->second.action == "EXECUTION" && it->second.originalQty - it->second.size == 0) {
            it->second.isPending = false;
            if (!it->second.isGuess)
                guesses.erase(it);
            return true;
        }

        if (it->second.action == "ADD" && it->second.isPending) {
            guesses.erase(it);
            return true;
        }
    }
    return false;
}

bool OrderBook::reconcileTrade(Price price, Quantity quantity) {
    while (!guessedExecutions.empty()) {
        OrderInfo* exec = guessedExecutions.front();
        guessedExecutions.pop();
        auto it = guesses.find(exec->orderId);
        if (it == guesses.end()) continue;

        if (exec->price == price && exec->size == quantity) {
            // validates the guess
            auto& execRec = it->second;
            if (execRec.action == "EXECUTION" && execRec.price == price && execRec.size == quantity) {
                execRec.isGuess = false;
                if (!execRec.isPending) 
                    guesses.erase(it);
            }
            return true;
        }
        // invalidate the trade guess if another trade is received
        auto& execRec = it->second;
        if (execRec.action == "EXECUTION" && execRec.isGuess) {
            execRec.isGuess = false;
            // report revision to downstream
            if (execRec.originalQty == execRec.size) {
                execRec.action = "CANCEL";
                onOrderCancel(*this, execRec);
            } else {
                execRec.action = "MODIFY";
                execRec.size = execRec.originalQty - execRec.size;
                onOrderModify(*this, execRec);
            }
            guesses.erase(it);
        }
    }

    for (auto it = guesses.begin(); it != guesses.end(); ) {
        OrderInfo& guess = it->second;
        if ((guess.action == "MODIFY" && quantity == guess.originalQty - guess.size
            || guess.action == "CANCEL" && quantity == guess.size) &&
            guess.isGuess &&
            guess.price == price
            ) {
            guess.isGuess = false;
            guess.size = quantity;
            guess.action = "EXECUTION";
            onOrderExecution(*this, guess);

            // Remove the invalid guess
            guesses.erase(it);
            return true;
        }
        ++it;
    }
    return false;
}

void OrderBook::guessNewOrder(Price price, Quantity size, bool isSell, bool isMarketable, 
    Timestamp timestamp, 
    bool isGuess) {

    static OrderId guessOrderId = -1;
    OrderId currId = guessOrderId--;
    if (!isMarketable) {
        smartBook.addOrder(currId, isSell, size, price);
    }

    OrderInfo newOrder(currId, isSell, price, size, "ADD", timestamp, size, true, isMarketable);
    if (isMarketable) {
        aggressors.push_back(newOrder);
    } else {
        newOrder.isGuess = isGuess;
        guesses.insert({currId, newOrder});
    }
    
    onOrderAdd(*this, newOrder);
}

void OrderBook::onExecution(Price price, Quantity quantity, Timestamp timestamp, bool isGuess) {
    auto [isMarketable, isSellAggressor] = deduceIsSellAggressor(price);

    // guess there is an aggressive order
    guessNewOrder(price, quantity, isSellAggressor, true, timestamp);

    // we can be sure that an execution has occured
    auto executions = smartBook.executeAtPrice(price, quantity, isGuess);

    for (auto exec : executions) {
        exec.timestamp = timestamp;
        auto [it, inserted] = guesses.emplace(exec.orderId, exec);
        if (isGuess) {
            guessedExecutions.push(&it->second);
        }
        onOrderExecution(*this, exec);
    }
}

// Returns {isMarketable, isSellAggressor}
std::pair<bool, bool> OrderBook::deduceIsSellAggressor(Price price) const {
    Price bid = smartBook.getBestBid();
    if (bid != 0 && price <= bid) {
        return {true, true};
    }

    Price ask = smartBook.getBestAsk();
    if (ask != 0 && price >= ask) {
        return {true, false};
    }

    return {false, false};
}

void OrderBook::onOrderAdd(OrderBook& smartOrderBook, const OrderInfo& orderInfo) {
    std::cout << "[Callback] [" << orderInfo.timestamp << "] "
        << orderInfo.action << (orderInfo.isGuess ? " (Guess): ": ": ") 
        << orderInfo.orderId << (orderInfo.isSell ? " SELL " : " BUY ")
        << orderInfo.size << " @ " << orderInfo.price << "\n";
}

void OrderBook::onOrderExecution(OrderBook& smartOrderBook, const OrderInfo& orderInfo) {
    std::cout << "[Callback] [" << orderInfo.timestamp << "] "
        << orderInfo.action << (orderInfo.isGuess ? " (Guess): ": ": ") 
        << orderInfo.orderId << " "
        //<< (orderInfo.isSell ? " SELL " : " BUY ")
        << orderInfo.size << " @ " << orderInfo.price << "\n";
}

void OrderBook::onOrderModify(OrderBook& smartOrderBook, const OrderInfo& orderInfo) {
    std::cout << "[Callback] [" << orderInfo.timestamp << "] "
        << orderInfo.action << (orderInfo.isGuess ? " (Guess): ": ": ") 
        << orderInfo.orderId << (orderInfo.isSell ? " SELL " : " BUY ")
        << orderInfo.size << " @ " << orderInfo.price << "\n";
}

void OrderBook::onOrderCancel(OrderBook& smartOrderBook, const OrderInfo& orderInfo) {
    std::cout << "[Callback] [" << orderInfo.timestamp << "] "
        << orderInfo.action << (orderInfo.isGuess ? " (Guess): ": ": ") 
        << orderInfo.orderId << (orderInfo.isSell ? " SELL " : " BUY ")
        << orderInfo.size << " @ " << orderInfo.price << "\n";
}