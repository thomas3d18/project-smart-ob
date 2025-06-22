file(GLOB TEST_SOURCES *.cpp)

add_executable(SmartOrderBookTests
    ${TEST_SOURCES}
    ../src/OrderBook.cpp
    ../src/L2Book.cpp
    ../src/L3Book.cpp
    ../src/TradeContainer.cpp
    #../src/Callbacks.cpp
)

target_include_directories(SmartOrderBookTests PRIVATE ../include)