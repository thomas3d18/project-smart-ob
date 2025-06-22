#include "MarketDataIngestor.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>


MarketDataIngestor::MarketDataIngestor(OrderBook& orderBook) : orderBook(orderBook) {}

void MarketDataIngestor::loadFile(const std::string& file, EventType type) {
    std::ifstream in(file);
    if (!in) {
        std::cout << "Failed to open " << file << "\n";
        return;
    }

    std::string line;
    while (std::getline(in, line)) {
        std::cout << line << "\n";
        std::istringstream ss(line);
        uint64_t ts;
        ss >> ts;
        std::string rest = line.substr(line.find(' ') + 1);
        events.push_back(MarketEvent {type, ts, rest});
    }
}

void MarketDataIngestor::loadEvents(const std::string& l2File, 
                                    const std::string& l3file, 
                                    const std::string& tradesFile) {
    loadFile(l2File, EventType::L2_SNAPSHOT);
    loadFile(l3file, EventType::L3_UPDATE);
    loadFile(tradesFile, EventType::TRADE_EXECUTION);

    std::sort(events.begin(), events.end(), 
        [](const MarketEvent& a, const MarketEvent& b) {
            return a.timestamp < b.timestamp;
    });

    std::cout << "=== Finished loading market data ===\n";
}

void MarketDataIngestor::processEvents() {
    for(const auto& e : events) {
        if (e.type == EventType::L2_SNAPSHOT) {
            std::cout << "[" << e.timestamp << "] [L2_SNAPSHOT] " << e.rawData << "\n";
            orderBook.processL2Snapshot(e.rawData, e.timestamp);
        } else if (e.type == EventType::L3_UPDATE) {
            std::cout << "[" << e.timestamp << "] [L3_UPDATE] " << e.rawData << "\n";
            orderBook.processL3Update(e.rawData, e.timestamp);
        } else if (e.type == EventType::TRADE_EXECUTION) {
            std::cout << "[" << e.timestamp << "] [TRADE] " << e.rawData << "\n";
            orderBook.processTrade(e.rawData, e.timestamp);
        }

    }
}

void MarketDataIngestor::parseL2Snapshot(const std::string& data, Timestamp timestamp) {
    std::istringstream ss(data);
    std::string token;


}