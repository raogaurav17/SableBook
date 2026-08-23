#pragma once

#include <chrono>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <iomanip>

namespace sablebook {

inline uint64_t getTimestampNs() noexcept {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

class LatencyTracker {
public:
    LatencyTracker() = default;

    void reserve(size_t capacity) {
        samples_.reserve(capacity);
    }

    void record(uint64_t latency_ns) {
        samples_.push_back(latency_ns);
    }

    void clear() {
        samples_.clear();
    }

    [[nodiscard]] size_t count() const noexcept { return samples_.size(); }

    struct LatencyStats {
        size_t count{0};
        double min_ns{0.0};
        double p50_ns{0.0};
        double p90_ns{0.0};
        double p95_ns{0.0};
        double p99_ns{0.0};
        double p99_9_ns{0.0};
        double max_ns{0.0};
        double mean_ns{0.0};
        double stddev_ns{0.0};
    };

    [[nodiscard]] LatencyStats getStats() const {
        if (samples_.empty()) {
            return {};
        }

        std::vector<uint64_t> sorted = samples_;
        std::sort(sorted.begin(), sorted.end());

        size_t n = sorted.size();
        double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
        double mean = sum / n;

        double accum = 0.0;
        for (uint64_t val : sorted) {
            accum += (val - mean) * (val - mean);
        }
        double stddev = std::sqrt(accum / n);

        auto percentile = [&](double p) -> double {
            size_t idx = static_cast<size_t>(p * n / 100.0);
            if (idx >= n) idx = n - 1;
            return static_cast<double>(sorted[idx]);
        };

        return LatencyStats{
            n,
            static_cast<double>(sorted.front()),
            percentile(50.0),
            percentile(90.0),
            percentile(95.0),
            percentile(99.0),
            percentile(99.9),
            static_cast<double>(sorted.back()),
            mean,
            stddev
        };
    }

    void printReport(const std::string& title = "Latency Report") const {
        LatencyStats stats = getStats();
        if (stats.count == 0) {
            std::cout << "[" << title << "] No samples recorded.\n";
            return;
        }

        std::cout << "\n======================================================\n";
        std::cout << "  " << title << " (N = " << stats.count << ")\n";
        std::cout << "======================================================\n";
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  Min:     " << std::setw(10) << stats.min_ns << " ns  (" << stats.min_ns / 1000.0 << " μs)\n";
        std::cout << "  p50:     " << std::setw(10) << stats.p50_ns << " ns  (" << stats.p50_ns / 1000.0 << " μs)\n";
        std::cout << "  p90:     " << std::setw(10) << stats.p90_ns << " ns  (" << stats.p90_ns / 1000.0 << " μs)\n";
        std::cout << "  p95:     " << std::setw(10) << stats.p95_ns << " ns  (" << stats.p95_ns / 1000.0 << " μs)\n";
        std::cout << "  p99:     " << std::setw(10) << stats.p99_ns << " ns  (" << stats.p99_ns / 1000.0 << " μs)\n";
        std::cout << "  p99.9:   " << std::setw(10) << stats.p99_9_ns << " ns  (" << stats.p99_9_ns / 1000.0 << " μs)\n";
        std::cout << "  Max:     " << std::setw(10) << stats.max_ns << " ns  (" << stats.max_ns / 1000.0 << " μs)\n";
        std::cout << "  Mean:    " << std::setw(10) << stats.mean_ns << " ns  (" << stats.mean_ns / 1000.0 << " μs)\n";
        std::cout << "  StdDev:  " << std::setw(10) << stats.stddev_ns << " ns  (" << stats.stddev_ns / 1000.0 << " μs)\n";
        std::cout << "======================================================\n\n";
    }

private:
    std::vector<uint64_t> samples_;
};

} // namespace sablebook
