#pragma once

#include "Types.hpp"
#include "Order.hpp"
#include "Trade.hpp"
#include "PriceLevel.hpp"

#include <map>
#include <unordered_map>
#include <vector>
#include <optional>
#include <functional>
#include <memory>
#include <string>

namespace sablebook {

struct OrderLocation {
    OrderPtr order;
    Side side;
    double price;
    PriceLevel::OrderIterator it;
};

class OrderBook {
public:
    using BidMap = std::map<double, PriceLevel, std::greater<double>>;
    using AskMap = std::map<double, PriceLevel, std::less<double>>;

    explicit OrderBook(std::string symbol = "DEFAULT");
    ~OrderBook() = default;

    // Non-copyable, movable
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) noexcept = default;
    OrderBook& operator=(OrderBook&&) noexcept = default;

    [[nodiscard]] const std::string& symbol() const noexcept { return symbol_; }

    // Core Matching & Order Operations
    std::vector<Trade> addOrder(const OrderPtr& order, uint64_t& next_trade_id);
    bool cancelOrder(uint64_t order_id);
    bool modifyOrder(uint64_t order_id, double new_price, uint64_t new_qty, uint64_t timestamp, uint64_t& next_trade_id, std::vector<Trade>& generated_trades);

    // Queries
    [[nodiscard]] std::optional<LevelView> bestBid() const;
    [[nodiscard]] std::optional<LevelView> bestAsk() const;
    [[nodiscard]] BBO getBBO() const;
    [[nodiscard]] std::vector<LevelView> getDepth(Side side, size_t levels) const;
    [[nodiscard]] OrderPtr getOrder(uint64_t order_id) const;
    [[nodiscard]] std::optional<OrderStatus> getOrderStatus(uint64_t order_id) const;

    [[nodiscard]] uint64_t totalBidVolume() const noexcept;
    [[nodiscard]] uint64_t totalAskVolume() const noexcept;
    [[nodiscard]] size_t bidLevelCount() const noexcept { return bids_.size(); }
    [[nodiscard]] size_t askLevelCount() const noexcept { return asks_.size(); }
    [[nodiscard]] size_t totalOrders() const noexcept { return order_lookup_.size(); }

    void clear();

private:
    void addRestingOrder(const OrderPtr& order);
    std::vector<Trade> executeMatch(const OrderPtr& aggressive_order, uint64_t& next_trade_id);

    std::string symbol_;
    BidMap bids_;
    AskMap asks_;
    std::unordered_map<uint64_t, OrderLocation> order_lookup_;
    std::unordered_map<uint64_t, OrderPtr> order_history_;
};

} // namespace sablebook
