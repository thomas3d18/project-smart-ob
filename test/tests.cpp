#include "OrderBook.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <functional>

class TestSuite {
public:
    void addTest(const std::string& name, std::function<void()> func) {
        tests.push_back({name, func});
    }

    void run() {
        int passed = 0;
        for (const auto& t : tests) {
            try {
                std::cout << "=== Running test: " << t.first << "...\n";
                t.second();
                std::cout << "=== [PASS] " << t.first << "\n";
                passed++;
            } catch (const std::exception& e) {
                std::cout << "=== [FAIL] " << t.first << ": " << e.what() << "\n";
            } catch (...) {
                std::cout << "=== [FAIL] " << t.first << ": Unknown error\n";
            }
        }
        std::cout << passed << " / " << tests.size() << " tests passed.\n";
    }

private:
    std::vector<std::pair<std::string, std::function<void()>>> tests;
};

#define ASSERT_TRUE(cond) if (!(cond)) throw std::runtime_error(#cond " failed")
#define ASSERT_EQ(a,b) if (!((a)==(b))) throw std::runtime_error(std::string(#a " != " #b))

// Example test
void test_l3_add_order() {
    L2Book l2;
    L3Book l3;
    TradeContainer trades;
    OrderBook ob(l2, l3, trades);

    ob.processL3Update("ADD 1 BUY 100.0 200", 1);

    ASSERT_EQ(l3.getBestBid(), 100.0);
    ASSERT_EQ(l3.getTopBids()[0].orders.front().size, 200);

    ASSERT_EQ(ob.getSmartOrderBook().getBestBid(), 100.0);
    ASSERT_EQ(ob.getSmartOrderBook().getTopBids()[0].orders.front().size, 200);
}

void test_multiple_order_executions() {
    L2Book l2;
    L3Book l3;
    TradeContainer trades;
    OrderBook ob(l2, l3, trades);

    ob.processL3Update("ADD 1 BUY 100.0 100", 1);
    ob.processL3Update("ADD 2 BUY 100.0 100", 2);
    ASSERT_EQ(ob.getSmartOrderBook().getTotalOrders(), 2);

    ob.processTrade("100.0 200", 3);

    ASSERT_TRUE(ob.getSmartOrderBook().empty());
    ASSERT_EQ(ob.getSmartOrderBook().getTotalOrders(), 0);
    ASSERT_EQ(ob.getAggressors().size(), 1);
}

void test_trade_leads_L3_sell_aggressive() {
    L2Book l2;
    L3Book l3;
    TradeContainer trades;
    OrderBook ob(l2, l3, trades);

    ob.processL3Update("ADD 1 BUY 100.0 100", 1);
    ob.processL3Update("ADD 2 BUY 100.0 100", 2);
    ASSERT_EQ(ob.getSmartOrderBook().getTotalOrders(), 2);

    ob.processTrade("100.0 200", 3);

    ASSERT_TRUE(ob.getSmartOrderBook().empty());
    ASSERT_EQ(ob.getSmartOrderBook().getTotalOrders(), 0);

    // lagged L3 updates confirm the addition/removals
    ob.processL3Update("ADD 3 SELL 100.0 200", 5);
    ob.processL3Update("CANCEL 3", 5);
    ob.processL3Update("CANCEL 1", 6);
    ob.processL3Update("CANCEL 2", 7);

    ASSERT_TRUE(ob.getSmartOrderBook().empty());
    ASSERT_EQ(ob.getSmartOrderBook().getTotalOrders(), 0);
}

void test_trade_leads_L3_buy_aggressive() {
    L2Book l2;
    L3Book l3;
    TradeContainer trades;
    OrderBook ob(l2, l3, trades);

    ob.processL3Update("ADD 1 SELL 101.0 100", 1);
    ob.processL3Update("ADD 2 BUY 100.0 100", 2);
    ASSERT_EQ(ob.getSmartOrderBook().getTotalOrders(), 2);

    ob.processTrade("101.0 100", 3);

    ASSERT_TRUE(ob.getSmartOrderBook().getTopAsks(1).empty());
    ASSERT_EQ(ob.getSmartOrderBook().getTotalOrders(), 1);
    ASSERT_EQ(ob.getAggressors().size(), 1);

    // lagged L3 updates confirm the addition/removals
    ob.processL3Update("ADD 3 BUY 101.0 100", 5);
    ob.processL3Update("CANCEL 3", 5);
    ob.processL3Update("CANCEL 1", 6);

    ASSERT_TRUE(ob.getSmartOrderBook().getTopAsks(1).empty());
    ASSERT_EQ(ob.getSmartOrderBook().getTotalOrders(), 1);
    ASSERT_EQ(ob.getAggressors().size(), 0);
}

void test_trade_leads_L3_partial_fill() {
    L2Book l2;
    L3Book l3;
    TradeContainer trades;
    OrderBook ob(l2, l3, trades);

    ob.processL3Update("ADD 1 BUY 100.0 100", 1);
    ob.processL3Update("ADD 2 BUY 100.0 100", 2);
    ASSERT_EQ(ob.getSmartOrderBook().getTotalOrders(), 2);

    ob.processTrade("100.0 160", 3);

    // Buy 50 @ 100.0 remains after match
    ASSERT_EQ(ob.getSmartOrderBook().getTopBids(1).front().price, 100.0);
    ASSERT_EQ(ob.getSmartOrderBook().getTopBids(1).front().quantity, 40);
    ASSERT_EQ(ob.getSmartOrderBook().getTotalOrders(), 1);

    // Two execution guesses
    ASSERT_EQ(ob.getGuesses().size(), 2);

    // We have recorded an aggressor for L3 reconciliation
    ASSERT_EQ(ob.getAggressors().size(), 1);

    // lagged L3 updates confirm our guess
    ob.processL3Update("ADD 3 SELL 100.0 160", 5);

    // Transferred marketable order from aggressors to guesses
    ASSERT_EQ(ob.getAggressors().size(), 0);
    ASSERT_EQ(ob.getGuesses().size(), 3);

    ob.processL3Update("CANCEL 3", 5);
    ob.processL3Update("CANCEL 1", 6);
    ob.processL3Update("MODIFY 2 BUY 100.0 40", 7);

    ASSERT_EQ(ob.getSmartOrderBook().getTopBids(1).front().price, 100.0);
    ASSERT_EQ(ob.getSmartOrderBook().getTopBids(1).front().quantity, 40);
    ASSERT_EQ(ob.getSmartOrderBook().getTotalOrders(), 1);

    // previous guesses are confirmed and removed
    ASSERT_EQ(ob.getGuesses().size(), 0);
}

void test_L2_leads_favour_execution_guess_trade_lags_valid() {
    L2Book l2;
    L3Book l3;
    TradeContainer trades;
    OrderBook ob(l2, l3, trades, 1);

    ob.processL3Update("ADD 1 BUY 100.0 500", 1);
    ob.processL3Update("ADD 2 SELL 101.0 500", 2);

    // Reduced qty on bid
    ob.processL2Snapshot("BID 100.0 300 ASK 101.0 500", 3);
    ASSERT_EQ(l2.getBestBid(), 100.0);
    ASSERT_EQ(l2.getBestAsk(), 101.0);
    ASSERT_EQ(l2.getBidQuantityAtPrice(100.0), 300);
    ASSERT_EQ(l2.getAskQuantityAtPrice(101.0), 500);

    ASSERT_EQ(ob.getSmartOrderBook().getTopBids(1).begin()->price, 100.0);
    ASSERT_EQ(ob.getSmartOrderBook().getTopAsks(1).begin()->price, 101.0);
    ASSERT_EQ(ob.getSmartOrderBook().getTopBids(1).begin()->quantity, 300.0);

    // We guess the reduction was due to execution
    ASSERT_EQ(ob.getGuesses().size(), 1);
    ASSERT_EQ(ob.getGuesses().begin()->second.action, "EXECUTION");

    // L3 arrives afterwards
    ob.processL3Update("MODIFY 1 BUY 100.0 300", 5);

    // Lagged trade update validates guess
    ob.processTrade("100.0 200", 4);

    // L3 book remains the same
    ASSERT_EQ(ob.getSmartOrderBook().getTopBids(1).begin()->price, 100.0);
    ASSERT_EQ(ob.getSmartOrderBook().getTopAsks(1).begin()->price, 101.0);
    ASSERT_EQ(ob.getSmartOrderBook().getTopBids(1).begin()->quantity, 300.0);

    // Entire bid gets hit
    ob.processL2Snapshot("BID ASK 101.0 500", 3);

    ASSERT_TRUE(ob.getSmartOrderBook().getTopBids(1).empty());

    // L3 arrives afterwards
    ob.processL3Update("CANCEL 1", 6);

    // Lagged trade update validates guess
    ob.processTrade("100.0 300", 5);

    // All guesses are validated
    ASSERT_EQ(ob.getGuesses().size(), 0);
}

void test_L2_leads_favour_execution_guess_l3_lags_valid() {
    L2Book l2;
    L3Book l3;
    TradeContainer trades;
    OrderBook ob(l2, l3, trades, 1);

    ob.processL3Update("ADD 1 BUY 100.0 500", 1);
    ob.processL3Update("ADD 2 SELL 101.0 500", 2);

    // Reduced qty on bid
    ob.processL2Snapshot("BID 100.0 300 ASK 101.0 500", 3);
    ASSERT_EQ(l2.getBestBid(), 100.0);
    ASSERT_EQ(l2.getBestAsk(), 101.0);
    ASSERT_EQ(l2.getBidQuantityAtPrice(100.0), 300);
    ASSERT_EQ(l2.getAskQuantityAtPrice(101.0), 500);

    ASSERT_EQ(ob.getSmartOrderBook().getTopBids(1).begin()->price, 100.0);
    ASSERT_EQ(ob.getSmartOrderBook().getTopAsks(1).begin()->price, 101.0);
    ASSERT_EQ(ob.getSmartOrderBook().getTopBids(1).begin()->quantity, 300.0);

    // We guess the reduction was due to execution
    ASSERT_EQ(ob.getGuesses().size(), 1);
    ASSERT_EQ(ob.getGuesses().begin()->second.action, "EXECUTION");

    // Trade update validates guess
    ob.processTrade("100.0 200", 4);

    // L3 lags behind
    ob.processL3Update("MODIFY 1 BUY 100.0 300", 5);


    // L3 book remains the same
    ASSERT_EQ(ob.getSmartOrderBook().getTopBids(1).begin()->price, 100.0);
    ASSERT_EQ(ob.getSmartOrderBook().getTopAsks(1).begin()->price, 101.0);
    ASSERT_EQ(ob.getSmartOrderBook().getTopBids(1).begin()->quantity, 300.0);

    // Entire bid gets hit
    ob.processL2Snapshot("BID ASK 101.0 500", 3);

    ASSERT_TRUE(ob.getSmartOrderBook().getTopBids(1).empty());

    // Trade update validates guess
    ob.processTrade("100.0 300", 5);

    // L3 lags behind
    ob.processL3Update("CANCEL 1", 6);

    // All guesses are validated
    ASSERT_EQ(ob.getGuesses().size(), 0);
}

void test_L2_leads_favour_execution_guess_invalid() {
    L2Book l2;
    L3Book l3;
    TradeContainer trades;
    OrderBook ob(l2, l3, trades, 1);

    ob.processL3Update("ADD 1 BUY 100.0 500", 1);
    ob.processL3Update("ADD 2 SELL 101.0 500", 2);

    // Reduced qty on bid
    ob.processL2Snapshot("BID 100.0 300 ASK 101.0 500", 3);

    // L3 arrives
    ob.processL3Update("MODIFY 1 BUY 100.0 300", 5);

    // We guessed the trade was due to execution but in fact was an amendmend
    // Keep guess as 1
    ASSERT_EQ(ob.getGuesses().size(), 1);

    // Subsequent trade occurs on the other side - invalidate the trade guess
    ob.processTrade("101.0 100", 6);
    ob.processL3Update("MODIFY 2 SELL 101.0 400", 6);

    ASSERT_EQ(ob.getSmartOrderBook().getTopAsks(1).front().quantity, 400.0);
    ASSERT_EQ(ob.getGuesses().size(), 0);

    ob.processL3Update("ADD 3 BUY 99.0 400", 7);
    ob.processL2Snapshot("BID 99.0 400 ASK 101.0 400 ", 7);

    ASSERT_EQ(ob.getGuesses().size(), 1);
    
    ob.processTrade("99.0 100", 8);
    ob.processL3Update("MODIFY 3 SELL 99.0 300", 8);

    ASSERT_EQ(ob.getGuesses().size(), 0);
}

void test_L2_leads_guess_modify_valid() {
    L2Book l2;
    L3Book l3;
    TradeContainer trades;
    OrderBook ob(l2, l3, trades, 0);

    ob.processL3Update("ADD 1 BUY 100.0 500", 1);
    ob.processL3Update("ADD 2 SELL 101.0 500", 2);

    // Reduced qty on bid
    ob.processL2Snapshot("BID 100.0 300 ASK 101.0 500", 3);

    ASSERT_EQ(ob.getSmartOrderBook().getTopBids(1).front().quantity, 300.0);
    ASSERT_EQ(ob.getGuesses().size(), 1);

    // L3 Validates
    ob.processL3Update("MODIFY 1 BUY 100.0 300", 4);
}

void test_L2_leads_guess_modify_invalid() {
    L2Book l2;
    L3Book l3;
    TradeContainer trades;
    OrderBook ob(l2, l3, trades, 0);

    ob.processL3Update("ADD 1 BUY 100.0 500", 1);
    ob.processL3Update("ADD 2 SELL 101.0 500", 2);

    // Reduced qty on bid
    ob.processL2Snapshot("BID 100.0 300 ASK 101.0 500", 3);

    ASSERT_EQ(ob.getSmartOrderBook().getTopBids(1).front().quantity, 300.0);
    ASSERT_EQ(ob.getGuesses().size(), 1);

    // L3 Validates
    ob.processL3Update("MODIFY 1 BUY 100.0 300", 4);
    ASSERT_EQ(ob.getGuesses().size(), 1);

    // Lagged trade invalidates guess
    ob.processTrade("100.0 200", 6);
    ASSERT_EQ(ob.getGuesses().size(), 0);

    // Bid cancelled
    ob.processL2Snapshot("BID ASK 101.0 500", 7);

    ASSERT_TRUE(ob.getSmartOrderBook().getTopBids(1).empty());
    ASSERT_EQ(ob.getGuesses().size(), 1);

    // Lagged trade invalidates guess
    ob.processTrade("100.0 300", 10);
    ASSERT_EQ(ob.getGuesses().size(), 0);
}

void test_L2_leads_added_qty() {
    L2Book l2;
    L3Book l3;
    TradeContainer trades;
    OrderBook ob(l2, l3, trades, 0);

    ob.processL3Update("ADD 1 BUY 100.0 500", 1);
    ob.processL3Update("ADD 2 SELL 101.0 500", 2);

    // New price level and added liquidity
    ob.processL2Snapshot("BID 100.5 300 100.0 800 ASK 101.0 500", 3);
    ASSERT_EQ(ob.getGuesses().size(), 2);

    // L3 validates
    ob.processL3Update("ADD 3 BUY 100.5 300", 4);
    ob.processL3Update("ADD 4 BUY 100 300", 4);
    ASSERT_EQ(ob.getGuesses().size(), 0);
}

void test_L2_leads_added_qty_invalid() {
    L2Book l2;
    L3Book l3;
    TradeContainer trades;
    OrderBook ob(l2, l3, trades, 0);

    ob.processL3Update("ADD 1 BUY 100.0 500", 1);
    ob.processL3Update("ADD 2 SELL 101.0 500", 2);

    // New price level and added liquidity
    ob.processL2Snapshot("BID 100.0 800 ASK 101.0 500", 3);
    ASSERT_EQ(ob.getGuesses().size(), 1);

    // L3 invalidates - the addition was in fact an amend
    ob.processL3Update("MODIFY 1 BUY 100.0 800", 4);
    ASSERT_EQ(ob.getSmartOrderBook().getTopBids(1).front().numOrders, 1);
    ASSERT_EQ(ob.getGuesses().size(), 0);
}

int main() {
    TestSuite suite;
    suite.addTest("L3 ADD update", test_l3_add_order);
    suite.addTest("Trade update - multiple orders executed", test_multiple_order_executions);
    suite.addTest("Trade leads L3 update SELL aggressive", test_trade_leads_L3_sell_aggressive);
    suite.addTest("Trade leads L3 update BUY aggressive", test_trade_leads_L3_buy_aggressive);
    suite.addTest("Trade leads L3 update with partial fills", test_trade_leads_L3_partial_fill);
    suite.addTest("L2 Snaphot Leads (Always guesses execution) - Trade lags valid", test_L2_leads_favour_execution_guess_trade_lags_valid);
    suite.addTest("L2 Snaphot Leads (Always guesses execution) - L3 lags valid", test_L2_leads_favour_execution_guess_l3_lags_valid);
    suite.addTest("L2 Snapshot Leads (Always gusses execution) - Wrong guess", test_L2_leads_favour_execution_guess_invalid);
    suite.addTest("L2 Snapshot Leads (Guess modify/cancel) - Valid", test_L2_leads_guess_modify_valid);
    suite.addTest("L2 Snapshot Leads (Guess modify/cancel) - Invalid", test_L2_leads_guess_modify_invalid);
    suite.addTest("L2 Snapshot Leads Add", test_L2_leads_added_qty);
    suite.addTest("L2 Snapshot Leads Add Invalid", test_L2_leads_added_qty_invalid);

    suite.run();
    return 0;
}
