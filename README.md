# SmartOrderBook

A Smart Level 3 Order Book engine designed to handle Level-2 (L2) and Level-3 (L3) order books, trade execution tracking, and out-of-sync market data reconciliation. The system makes best educated guesses, maintains an uncrossed order book views, even when market data streams arrive out of sync.

---

## Overview

This project implements:
- Real-time Level-2 and Level-3 order books.
- A trade container to track trade events.
- Smart guessing logic to handle out-of-sync market data and reconcile when actual market data updates arrive.

---

## Guessing logic, design decisions, and assumptions

### Assumptions

The following market data format is assumed:
- L2 Full snapshots
    - ```<timestamp> BID <bid_0 price> <bid_0 size> ... <bid_n price> <bid_n size> ASK <ask_0 price> <ask_0 size> ... <ask_n price> <ask_n size>```
    - L2 order book is assumed to not be crossed at all times
- L3 order updates
    - ```<timestamp> <ADD> <order id> <side> <price> <size>```
    - ```<timestamp> <MODIFY> <order id> <side> <price> <size>```
    - ```<timestamp> <CANCEL> <order id>```
- Trade execution
    - ```<timestamp> <price> <size>```
- To demonstrate the guessing/reconciliation logic, minimal information is assumed for L3 order and trade execution updates. For instance, there are no execution updates in L3 and no aggressor side or order id is provided in trade updates.
- While different market feeds may become unsynchronized, updates within the same stream is assumed to be in correct sequence. We also assume no data loss or ill formed data in market data feeds, such that each order book action can be validated by a subsequent update. (e.g. A Trade update will always be followed by a MODIFY/CANCEL, albeit timing differences)

### Guessing logic

When L2 data leads:
- If L2 quantity reduces:
  - We guess either an execution (trade fill) or a cancel/amend down.
  - We apply a heuristic: approximately 30% of the time assume execution, 70% cancel or amend down.
  - This can be further validated/invalidated when a trade execution is received.
  - Since individual trade updates are assumed to be syncrhonized, trade guesses are immediately invalidated when it is not found in the subsequent trade updates.
- If L2 quantity increases or a new price level appears:
  - We guess that new order(s) have been added with dummy order IDs and insert them into the Smart L3 book.
  - This can be further validated when we receive L3 

When trade data leads:
  - We will immediately apply the execution to the smart order book and notify downstream.
  - This can be later validated by L3 order updates.

When L3 data leads:
  - If we see L3 quantity reduces:
    - We guess either an execution or a cancel/amend down using the same 30%/70% heuristic.
    - This will be later validated by subsequent trade updates.

When L3 data catches up:
  - If L3 contradicts the guess (e.g. no matching ADD arrives, or CANCEL arrives without a trade), we issue corrective actions (remove dummy, update downstream).

### Potential Improvements

- The following implementations are simplified due to time constraints and shall be enhanced if time permits: 
  - Only one pending action/guess per order is stored with a unorderedmap<OrderId, OrderInfo>. This can be enhanced to a map of order id to deque<OrderInfo> to store multiple pending actions to handle more complex scenarios while offering efficient operations at both ends of the queue and good cache locality, while preserving insertion order for reconciliation.
  - TradeContainer can be implemented as a deque<TradeInfo> to support aggregating trade quantities over a recent window. This enables us to integrate more heuristics to invalidate trade guesses by imposing a time lag window.
- std::map is a red-black tree that provides O(log N) operations but with poor cache locality. Sorted vectors with binary search or flat hash maps provide better cache locality and lower memory overhead. Concurrent skip lists can also be useful in a multi-threaded setup.
- L3 order books may contain thousands to millions of orders, and each order is typically allocate individually in standard STL containers, incurring allocator overhead. The use of a memory/order pool or allocator, pre-allocating memory for orders and reusing memory for canceled orders can reduce alloc/dealloc overhead and memory fragmentation.
- std::list provides O(1) insertion/removal but incurs memory overhead and pointer chasing as a doubly linked list. Use of a intrusive list helps to eliminate the extra allocation overhead and improves cache locality.
- In a real world setting, it is better to avoid duplication of price and quantity data by the L2 and L3 books. A shared reference to each price level can be maintained. L2 levels can also be composed from shared L3 levels to avodi data duplication.
- A lazy clean-up strategy can be employed on guesses, dummy orders or short-lived orders. First marking such data as likely executed then cleaned up in a batch can improve performance when message rate is high.

### How to build and run

Prerequisites
- CMake version 3.10 or higher
- C++17 compatible compiler (e.g. g++, clang++)

#### Build with CMake
```
mkdir -p build
cd build
cmake ..
make
```

#### Run the main Smart Order Book

```./SmartOrderBook```

Sample data files are stored under data/. They can be edited to simulate market data updates

#### Run the unit tests

```
cd ../test
./SmartOrderBookTest
```

## Declarations

1. The code is my own original work.
2. It does not contain or derive from confidential information belonging to any third party.
3. I am not violating any agreement or obligation with a current or past employer.