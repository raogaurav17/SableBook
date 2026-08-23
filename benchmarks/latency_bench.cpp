#include "MatchingEngine.hpp"
#include "Metrics.hpp"

#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <iomanip>

using namespace sablebook;

void runLimitOrderInsertBenchmark(size_t num_orders) {
    MatchingEngine engine;
    LatencyTracker tracker;
    tracker.reserve(num_orders);

    std::mt19937_64 rng(42);
    std::uniform_real_distribution<double> bid_dist(90.0, 99.99);
    std::uniform_real_distribution<double> ask_dist(100.01, 110.0);
    std::uniform_int_distribution<uint64_t> qty_dist(1, 100);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < num_orders; ++i) {
        Side side = (i % 2 == 0) ? Side::Buy : Side::Sell;
        double price = (side == Side::Buy) ? bid_dist(rng) : ask_dist(rng);
        uint64_t qty = qty_dist(rng);

        uint64_t t0 = getTimestampNs();
        engine.submitLimitOrder(side, price, qty);
        uint64_t t1 = getTimestampNs();

        tracker.record(t1 - t0);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double total_sec = std::chrono::duration<double>(end_time - start_time).count();
    double ops_per_sec = static_cast<double>(num_orders) / total_sec;

    std::cout << ">>> BENCHMARK 1: Limit Order Insertion (" << num_orders << " orders)\n";
    std::cout << "    Total Time: " << std::fixed << std::setprecision(3) << total_sec << " s\n";
    std::cout << "    Throughput: " << std::fixed << std::setprecision(0) << ops_per_sec << " orders/sec\n";
    tracker.printReport("Limit Order Insertion Latency");
}

void runCancellationBenchmark(size_t num_orders) {
    MatchingEngine engine;
    LatencyTracker tracker;
    tracker.reserve(num_orders);

    std::mt19937_64 rng(123);
    std::uniform_real_distribution<double> price_dist(10.0, 50.0);
    std::uniform_int_distribution<uint64_t> qty_dist(1, 100);

    std::vector<uint64_t> order_ids;
    order_ids.reserve(num_orders);

    for (size_t i = 0; i < num_orders; ++i) {
        order_ids.push_back(engine.submitLimitOrder(Side::Buy, price_dist(rng), qty_dist(rng)));
    }

    // Shuffle order IDs to simulate random cancellation order
    std::shuffle(order_ids.begin(), order_ids.end(), rng);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (uint64_t oid : order_ids) {
        uint64_t t0 = getTimestampNs();
        engine.cancelOrder(oid);
        uint64_t t1 = getTimestampNs();

        tracker.record(t1 - t0);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double total_sec = std::chrono::duration<double>(end_time - start_time).count();
    double ops_per_sec = static_cast<double>(num_orders) / total_sec;

    std::cout << ">>> BENCHMARK 2: Order Cancellation (" << num_orders << " cancels)\n";
    std::cout << "    Total Time: " << std::fixed << std::setprecision(3) << total_sec << " s\n";
    std::cout << "    Throughput: " << std::fixed << std::setprecision(0) << ops_per_sec << " cancels/sec\n";
    tracker.printReport("Order Cancellation Latency");
}

void runActiveMatchingBenchmark(size_t num_pairs) {
    MatchingEngine engine;
    LatencyTracker tracker;
    tracker.reserve(num_pairs);

    std::mt19937_64 rng(777);
    std::uniform_real_distribution<double> price_dist(100.0, 105.0);
    std::uniform_int_distribution<uint64_t> qty_dist(10, 50);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < num_pairs; ++i) {
        double p = price_dist(rng);
        uint64_t q = qty_dist(rng);

        // Pre-place resting ask
        engine.submitLimitOrder(Side::Sell, p, q);

        // Aggressive buy matching resting ask
        uint64_t t0 = getTimestampNs();
        engine.submitLimitOrder(Side::Buy, p, q);
        uint64_t t1 = getTimestampNs();

        tracker.record(t1 - t0);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double total_sec = std::chrono::duration<double>(end_time - start_time).count();
    double ops_per_sec = static_cast<double>(num_pairs * 2) / total_sec;

    std::cout << ">>> BENCHMARK 3: Active Matching Execution (" << num_pairs << " aggressive orders)\n";
    std::cout << "    Total Time: " << std::fixed << std::setprecision(3) << total_sec << " s\n";
    std::cout << "    Throughput: " << std::fixed << std::setprecision(0) << ops_per_sec << " orders/sec\n";
    tracker.printReport("Matching Execution Latency");
}

void runRealisticMixedWorkloadBenchmark(size_t total_events) {
    MatchingEngine engine;
    LatencyTracker tracker;
    tracker.reserve(total_events);

    std::mt19937_64 rng(999);
    std::uniform_int_distribution<int> event_dist(1, 100);
    std::uniform_real_distribution<double> price_base(100.0, 102.0);
    std::uniform_int_distribution<uint64_t> qty_dist(5, 50);

    std::vector<uint64_t> active_orders;
    active_orders.reserve(total_events / 2);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < total_events; ++i) {
        int ev = event_dist(rng);

        uint64_t t0 = getTimestampNs();

        if (ev <= 60) {
            // 60% Limit Orders
            Side side = (rng() % 2 == 0) ? Side::Buy : Side::Sell;
            double p = price_base(rng);
            uint64_t q = qty_dist(rng);
            uint64_t id = engine.submitLimitOrder(side, p, q);
            if (id > 0) active_orders.push_back(id);
        } else if (ev <= 85 && !active_orders.empty()) {
            // 25% Cancels
            size_t idx = rng() % active_orders.size();
            uint64_t id = active_orders[idx];
            engine.cancelOrder(id);
            active_orders[idx] = active_orders.back();
            active_orders.pop_back();
        } else if (ev <= 95) {
            // 10% Market Orders
            Side side = (rng() % 2 == 0) ? Side::Buy : Side::Sell;
            uint64_t q = qty_dist(rng);
            engine.submitMarketOrder(side, q);
        } else if (!active_orders.empty()) {
            // 5% Modifies
            size_t idx = rng() % active_orders.size();
            uint64_t id = active_orders[idx];
            engine.modifyOrder(id, price_base(rng), qty_dist(rng));
        }

        uint64_t t1 = getTimestampNs();
        tracker.record(t1 - t0);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double total_sec = std::chrono::duration<double>(end_time - start_time).count();
    double ops_per_sec = static_cast<double>(total_events) / total_sec;

    std::cout << ">>> BENCHMARK 4: Mixed Realistic Trading Workload (" << total_events << " events)\n";
    std::cout << "    Total Trades Executed: " << engine.totalTradesGenerated() << "\n";
    std::cout << "    Total Time: " << std::fixed << std::setprecision(3) << total_sec << " s\n";
    std::cout << "    Throughput: " << std::fixed << std::setprecision(0) << ops_per_sec << " events/sec\n";
    tracker.printReport("Mixed Workload Latency");
}

int main() {
    std::cout << "======================================================\n";
    std::cout << "        SableBook Performance Benchmark Suite         \n";
    std::cout << "======================================================\n\n";

    constexpr size_t N = 200'000;

    runLimitOrderInsertBenchmark(N);
    runCancellationBenchmark(N);
    runActiveMatchingBenchmark(N);
    runRealisticMixedWorkloadBenchmark(N);

    return 0;
}
