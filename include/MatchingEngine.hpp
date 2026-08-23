#pragma once

#include "Types.hpp"
#include "Order.hpp"
#include "Trade.hpp"
#include "OrderBook.hpp"
#include "Metrics.hpp"

#include <unordered_map>
#include <memory>
#include <string>
#include <functional>
#include <vector>
#include <optional>

namespace sablebook {

class MatchingEngine {
public:
    using TradeCallback = std::function<void(const Trade&)>;
    using BookUpdateCallback = std::function<void(const std::string&, const BBO&)>;
    using RejectCallback = std::function<void(uint64_t, RejectReason)>;

    explicit MatchingEngine(uint64_t max_order_quantity = 1'000'000'000);
    ~MatchingEngine() = default;

    // Non-copyable
    MatchingEngine(const MatchingEngine&) = delete;
    MatchingEngine& operator=(const MatchingEngine&) = delete;

    // Order Submission
    uint64_t submitOrder(const std::string& symbol,
                         Side side,
                         OrderType type,
                         double price,
                         uint64_t quantity,
                         RejectReason* reject_reason = nullptr);

    // Convenience overloads for default symbol
    uint64_t submitLimitOrder(Side side, double price, uint64_t quantity, const std::string& symbol = "DEFAULT");
    uint64_t submitMarketOrder(Side side, uint64_t quantity, const std::string& symbol = "DEFAULT");

    // Order Cancellation & Modification
    bool cancelOrder(uint64_t order_id);
    bool modifyOrder(uint64_t order_id, double new_price, uint64_t new_qty);

    // Callbacks & Market Data Publishing
    void setTradeCallback(TradeCallback cb) { on_trade_ = std::move(cb); }
    void setBookUpdateCallback(BookUpdateCallback cb) { on_book_update_ = std::move(cb); }
    void setRejectCallback(RejectCallback cb) { on_reject_ = std::move(cb); }

    // Instrument Management
    void registerSymbol(const std::string& symbol);
    [[nodiscard]] bool hasSymbol(const std::string& symbol) const;

    // Queries
    [[nodiscard]] BBO getBBO(const std::string& symbol = "DEFAULT") const;
    [[nodiscard]] std::vector<LevelView> getDepth(Side side, size_t levels = 5, const std::string& symbol = "DEFAULT") const;
    [[nodiscard]] std::optional<OrderStatus> getOrderStatus(uint64_t order_id) const;
    [[nodiscard]] OrderPtr getOrder(uint64_t order_id) const;

    [[nodiscard]] const OrderBook* getOrderBook(const std::string& symbol = "DEFAULT") const;
    [[nodiscard]] OrderBook* getOrderBook(const std::string& symbol = "DEFAULT");

    // Metrics & Stats
    [[nodiscard]] uint64_t totalOrdersProcessed() const noexcept { return next_order_id_ - 1; }
    [[nodiscard]] uint64_t totalTradesGenerated() const noexcept { return next_trade_id_ - 1; }
    [[nodiscard]] LatencyTracker& latencyTracker() noexcept { return latency_tracker_; }
    [[nodiscard]] const LatencyTracker& latencyTracker() const noexcept { return latency_tracker_; }

    void reset();

private:
    [[nodiscard]] OrderBook* getOrCreateBook(const std::string& symbol);

    uint64_t next_order_id_{1};
    uint64_t next_trade_id_{1};
    uint64_t max_order_quantity_{1'000'000'000};

    std::unordered_map<std::string, std::unique_ptr<OrderBook>> books_;
    std::unordered_map<uint64_t, std::string> order_symbol_map_;
    std::unordered_map<uint64_t, OrderPtr> terminal_orders_;

    TradeCallback on_trade_;
    BookUpdateCallback on_book_update_;
    RejectCallback on_reject_;

    LatencyTracker latency_tracker_;
};

} // namespace sablebook
