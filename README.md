# LOBster — Limit Order Book & Matching Engine

A high-performance, deterministic Limit Order Book (LOB) and Matching Engine written in **Modern C++ (C++20)**.

---

## Highlights & Performance

- **Matching Latency**: **~1.06 μs** mean latency (**0.20 μs** p50, **0.47 μs** p99).
- **Throughput**: **> 1,500,000 orders/sec** in active matching mode (**> 1,000,000 events/sec** in mixed trading workloads).
- **Strict FIFO Price-Time Priority**: Deterministic matching with resting order price setting and exact queue tracking.
- **$O(1)$ Fast Order Cancellation**: Iterator-indexed lookup enables instant cancellation from any queue position (head, middle, or tail).
- **Zero Memory Leaks**: Clean RAII ownership model using modern C++ smart pointers and STL containers.

---

## Features

### Core Requirements
| ID | Feature | Description | Status |
|---|---|---|---|
| **FR-1** | Accept Limit Orders | Supports Buy and Sell limit orders with price and quantity | |
| **FR-2** | Accept Market Orders | Matches against available liquidity, discarding unfilled balance | |
| **FR-3** | Price-Time Priority | Matches highest bid / lowest ask first; FIFO queue per price level | |
| **FR-4** | Partial Fills | Supports partial execution on resting and incoming aggressive orders | |
| **FR-5** | Order Cancellation | $O(1)$ cancellation by `order_id` from anywhere in the book | |
| **FR-6** | Status Tracking | Tracks `New`, `PartiallyFilled`, `Filled`, `Cancelled`, `Rejected` | |
| **FR-7** | Trade Events | Generates trade records with timestamp, price, and aggressive side | |
| **FR-8** | BBO Queries | Instant Best Bid and Best Offer (BBO), spread, and mid-price queries | |
| **FR-9** | Book Depth | Top-$N$ price level aggregations (price, total quantity, order count) | |
| **FR-10** | Multiple Orders / Level | FIFO queue with aggregate volume tracking per price level | |

### Advanced Features
| ID | Feature | Description | Status |
|---|---|---|---|
| **FR-11** | Amend / Modify Order | In-place volume reduction (keeps priority) or price/size change | |
| **FR-12** | Multi-Instrument Support | Isolated order books per symbol (e.g. `BTC-USD`, `ETH-USD`, `AAPL`) | |
| **FR-13** | Market Data Publishing | Callbacks for `Trade`, `BookUpdate` (BBO), and `OrderRejected` | |
| **FR-14** | Order Validation | Automatic rejection of zero/negative prices, zero quantities, etc. | |
| **FR-15** | Risk Limits | Configurable maximum order size enforcement | |

---

## Architecture & Design

```
                     ┌─────────────────────────────┐
                     │     Client / Interactive    │
                     │         Simulator           │
                     └──────────────┬──────────────┘
                                    │
                                    ▼
                     ┌─────────────────────────────┐
                     │       Order Gateway         │
                     │  - Validation & Risk Checks │
                     │  - Order ID & Timestamp Gen │
                     └──────────────┬──────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                        Matching Engine (Core)                          │
│  ┌────────────────────┐   ┌─────────────────┐   ┌───────────────────┐  │
│  │    Order Manager   │   │   Order Book    │   │  Matching Logic   │  │
│  │ (order_lookup_ map)│◄─►│  (Bids & Asks)  │◄─►│(FIFO Price-Time)  │  │
│  └────────────────────┘   └─────────────────┘   └───────────────────┘  │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
               ┌────────────────────┼────────────────────┐
               ▼                    ▼                    ▼
      ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
      │  Trade Callback │  │   Market Data   │  │ Latency Metrics │
      │   (on_trade_)   │  │ (on_book_update)│  │ (Percentiles)   │
      └─────────────────┘  └─────────────────┘  └─────────────────┘
```

### Data Structures
- **Bids**: `std::map<double, PriceLevel, std::greater<double>>` (Highest bid at root)
- **Asks**: `std::map<double, PriceLevel, std::less<double>>` (Lowest ask at root)
- **PriceLevel**: Contains `std::list<OrderPtr>` for FIFO time priority and $O(1)$ iterator erase
- **Order Lookup**: `std::unordered_map<uint64_t, OrderLocation>` storing direct iterators into the `PriceLevel` queues

---

## Benchmark Results

*Run on AMD/Intel x86_64, GCC 16, Release mode (`-O3 -march=native`):*

```
======================================================
        LOBster Performance Benchmark Suite           
======================================================

>>> BENCHMARK 1: Limit Order Insertion (200,000 orders)
    Throughput: 255,328 orders/sec
    Latency:    p50 = 1.98 μs | p90 = 4.12 μs | p99 = 38.14 μs | Mean = 3.85 μs

>>> BENCHMARK 2: Order Cancellation (200,000 cancels)
    Throughput: 323,251 cancels/sec
    Latency:    p50 = 2.48 μs | p90 = 3.58 μs | p99 = 5.25 μs  | Mean = 3.07 μs

>>> BENCHMARK 3: Active Matching Execution (200,000 aggressive orders)
    Throughput: 1,542,979 orders/sec
    Latency:    p50 = 0.20 μs | p90 = 0.28 μs | p99 = 0.47 μs  | Mean = 1.06 μs

>>> BENCHMARK 4: Mixed Realistic Trading Workload (200,000 events)
    Throughput: 1,097,337 events/sec
    Latency:    p50 = 0.54 μs | p90 = 1.19 μs | p99 = 2.41 μs  | Mean = 0.88 μs
======================================================
```

---

## Directory Structure

```text
LOBster/
├── include/
│   ├── Types.hpp            # Enums (Side, OrderType, OrderStatus), BBO, LevelView
│   ├── Order.hpp            # Order struct & shared pointer definitions
│   ├── Trade.hpp            # Trade event structure
│   ├── PriceLevel.hpp       # FIFO price level queue and aggregate volume
│   ├── OrderBook.hpp        # Core Limit Order Book (Bids, Asks, Matching)
│   ├── MatchingEngine.hpp   # Top-level engine with symbol routing & callbacks
│   └── Metrics.hpp          # Latency tracker, throughput timers & percentiles
├── src/
│   ├── OrderBook.cpp        # OrderBook implementation
│   ├── MatchingEngine.cpp   # MatchingEngine implementation
│   └── main.cpp             # Interactive CLI with ANSI color depth ladder & sim
├── tests/
│   ├── test_framework.hpp   # Lightweight unit test framework
│   ├── test_orderbook.cpp   # BBO, depth queries, and book state tests
│   ├── test_matching.cpp    # Limit/Market matching, partial fills, FIFO priority
│   ├── test_cancel.cpp      # O(1) cancel at head, middle, tail
│   ├── test_edge_cases.cpp  # Validation, rejections, modifications, multi-symbol
│   └── test_main.cpp        # Test suite entry point
├── benchmarks/
│   └── latency_bench.cpp    # Microsecond benchmark suite
├── CMakeLists.txt           # CMake build definition (C++20)
└── README.md                # Project documentation
```

---

## Building and Running

### Prerequisites
- C++20 compliant compiler (GCC 10+, Clang 11+, or MSVC 2019+)
- CMake 3.20+

### Build Instructions
```bash
# Clone and enter directory
cd LOBster

# Configure and compile
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Running Unit Tests
```bash
./build/lobster_tests
```

### Running Latency & Throughput Benchmarks
```bash
./build/lobster_bench
```

### Running Interactive CLI
```bash
./build/lobster_cli
```

#### CLI Commands
- `limit buy <price> <qty> [symbol]` — Submit a Limit Buy order
- `limit sell <price> <qty> [symbol]` — Submit a Limit Sell order
- `market buy <qty> [symbol]` — Submit a Market Buy order
- `market sell <qty> [symbol]` — Submit a Market Sell order
- `cancel <order_id>` — Cancel an active order
- `modify <order_id> <price> <qty>` — Modify order price or quantity
- `status <order_id>` — Inspect current order status and fill details
- `book [symbol] [levels]` — View real-time order book depth ladder
- `bbo [symbol]` — Query current Best Bid, Best Offer, and Spread
- `sim [count]` — Run a live simulated order flow
- `stats` — Print matching latency histogram and throughput stats
- `help` / `exit` — Help and quit commands
