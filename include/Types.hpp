#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include <vector>
#include <iostream>

namespace lobster {

enum class Side : uint8_t {
    Buy,
    Sell
};

inline std::string toString(Side side) {
    switch (side) {
        case Side::Buy:  return "Buy";
        case Side::Sell: return "Sell";
    }
    return "Unknown";
}

enum class OrderType : uint8_t {
    Limit,
    Market
};

inline std::string toString(OrderType type) {
    switch (type) {
        case OrderType::Limit:  return "Limit";
        case OrderType::Market: return "Market";
    }
    return "Unknown";
}

enum class OrderStatus : uint8_t {
    New,
    PartiallyFilled,
    Filled,
    Cancelled,
    Rejected
};

inline std::string toString(OrderStatus status) {
    switch (status) {
        case OrderStatus::New:             return "New";
        case OrderStatus::PartiallyFilled: return "PartiallyFilled";
        case OrderStatus::Filled:          return "Filled";
        case OrderStatus::Cancelled:       return "Cancelled";
        case OrderStatus::Rejected:        return "Rejected";
    }
    return "Unknown";
}

enum class RejectReason : uint8_t {
    None,
    InvalidPrice,
    InvalidQuantity,
    OrderNotFound,
    OrderAlreadyTerminal,
    MaxOrderSizeExceeded,
    InstrumentNotFound
};

inline std::string toString(RejectReason reason) {
    switch (reason) {
        case RejectReason::None:                  return "None";
        case RejectReason::InvalidPrice:         return "InvalidPrice";
        case RejectReason::InvalidQuantity:      return "InvalidQuantity";
        case RejectReason::OrderNotFound:        return "OrderNotFound";
        case RejectReason::OrderAlreadyTerminal: return "OrderAlreadyTerminal";
        case RejectReason::MaxOrderSizeExceeded: return "MaxOrderSizeExceeded";
        case RejectReason::InstrumentNotFound:   return "InstrumentNotFound";
    }
    return "Unknown";
}

struct LevelView {
    double price{0.0};
    uint64_t total_quantity{0};
    size_t order_count{0};
};

struct BBO {
    std::optional<LevelView> best_bid;
    std::optional<LevelView> best_ask;

    [[nodiscard]] bool hasBid() const noexcept { return best_bid.has_value(); }
    [[nodiscard]] bool hasAsk() const noexcept { return best_ask.has_value(); }
    
    [[nodiscard]] std::optional<double> spread() const noexcept {
        if (hasBid() && hasAsk()) {
            return best_ask->price - best_bid->price;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<double> midPrice() const noexcept {
        if (hasBid() && hasAsk()) {
            return (best_ask->price + best_bid->price) * 0.5;
        }
        return std::nullopt;
    }
};

} // namespace lobster
