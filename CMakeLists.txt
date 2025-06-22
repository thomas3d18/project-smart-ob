cmake_minimum_required(VERSION 3.10)
project(SmartOrderBook)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set (SOURCES
    src/L2Book.cpp
    src/L3Book.cpp
    src/MarketDataIngestor.cpp
    src/OrderBook.cpp
    src/TradeContainer.cpp
)

set (HEADERS
    include/OrderBook.hpp
    include/MarketDataIngestor.hpp
    #include/Callbacks.hpp
    include/DataStructure.hpp
    include/L2Book.hpp
    include/L3Book.hpp
    include/TradeContainder.hpp
    include/Types.hpp
)

include_directories(include)

add_executable(SmartOrderBook
    ${SOURCES}
    src/main.cpp
    #src/Reconciliation.cpp
    #src/Callbacks.cpp
)

add_subdirectory(test)
# add_executable(TestScenarios
#     #test/test_scenarios.cpp
#     src/MarketDataIngestor.cpp
#     src/OrderBook.cpp
#     #src/Reconciliation.cpp
#     #src/Callbacks.cpp
# )