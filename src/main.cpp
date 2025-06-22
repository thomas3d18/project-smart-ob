#include "MarketDataIngestor.hpp"
#include "OrderBook.hpp"
#include <iostream>

int main() {
    L2Book l2Book;
    L3Book L3Book;
    L3Book.name = "L3Book";
    TradeContainer trades;
    OrderBook smartOrderBook(l2Book, L3Book, trades);
    MarketDataIngestor ingestor(smartOrderBook);
    ingestor.loadEvents("../data/sample_L2.txt", "../data/sample_L3.txt", "../data/sample_trades.txt");
    ingestor.processEvents();
    std::cout << "Success" << std::endl;
    return 0;
}