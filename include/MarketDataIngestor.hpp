#pragma once
#include "OrderBook.hpp"
#include "Types.hpp"


class MarketDataIngestor {
public:
    explicit MarketDataIngestor(OrderBook& orderBook);

    void loadEvents(const std::string& l2File,
                    const std::string& l3File,
                    const std::string& tradeFile);

    void processEvents();

//private:
    OrderBook& orderBook;
    std::vector<MarketEvent> events;

    void loadFile(const std::string& file, EventType type);

    void parseL2Snapshot(const std::string& data, Timestamp timestamp);
};