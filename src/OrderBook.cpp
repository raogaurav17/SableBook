#include "OrderBook.hpp"
#include <algorithm>
#include <cmath>

namespace lobster {

OrderBook::OrderBook(std::string symbol)
    : symbol_(std::move(symbol)) {}

std::vector<Trade> OrderBook::addOrder(const OrderPtr& order, uint64_t& next_trade_id) {
    if (!order) {
        return {};
    }

    order_history_[order->order_id] = order;

    std::vector<Trade> trades = executeMatch(order, next_trade_id);

    if (order->type == OrderType::Limit && order->remaining_quantity > 0) {
        addRestingOrder(order);
    } else if (order->type == OrderType::Market && order->remaining_quantity > 0) {
        // Market orders do not rest on the book. Unfilled portion is cancelled.
        if (order->remaining_quantity == order->quantity) {
            order->status = OrderStatus::Cancelled;
        } else {
            // Partially filled, remaining is discarded
            order->status = OrderStatus::PartiallyFilled;
        }
    }

    return trades;
}

std::vector<Trade> OrderBook::executeMatch(const OrderPtr& aggressive_order, uint64_t& next_trade_id) {
    std::vector<Trade> trades;

    if (aggressive_order->side == Side::Buy) {
        while (aggressive_order->remaining_quantity > 0 && !asks_.empty()) {
            auto best_ask_it = asks_.begin();
            double ask_price = best_ask_it->first;

            if (aggressive_order->type == OrderType::Limit && aggressive_order->price < ask_price) {
                break; // Limit price below best ask; no match
            }

            PriceLevel& level = best_ask_it->second;

            while (aggressive_order->remaining_quantity > 0 && !level.empty()) {
                OrderPtr resting_order = level.frontOrder();
                if (!resting_order) {
                    level.popFront();
                    continue;
                }

                uint64_t match_qty = std::min(aggressive_order->remaining_quantity, resting_order->remaining_quantity);
                double match_price = resting_order->price; // Resting order sets the price

                aggressive_order->remaining_quantity -= match_qty;
                resting_order->remaining_quantity -= match_qty;
                level.reduceQuantity(match_qty);

                if (aggressive_order->remaining_quantity == 0) {
                    aggressive_order->status = OrderStatus::Filled;
                } else {
                    aggressive_order->status = OrderStatus::PartiallyFilled;
                }

                if (resting_order->remaining_quantity == 0) {
                    resting_order->status = OrderStatus::Filled;
                } else {
                    resting_order->status = OrderStatus::PartiallyFilled;
                }

                trades.emplace_back(
                    next_trade_id++,
                    aggressive_order->order_id,
                    resting_order->order_id,
                    symbol_,
                    match_price,
                    match_qty,
                    aggressive_order->timestamp,
                    Side::Buy
                );

                if (resting_order->isFilled()) {
                    order_lookup_.erase(resting_order->order_id);
                    level.popFront();
                }
            }

            if (level.empty()) {
                asks_.erase(best_ask_it);
            }
        }
    } else { // Side::Sell
        while (aggressive_order->remaining_quantity > 0 && !bids_.empty()) {
            auto best_bid_it = bids_.begin();
            double bid_price = best_bid_it->first;

            if (aggressive_order->type == OrderType::Limit && aggressive_order->price > bid_price) {
                break; // Limit price above best bid; no match
            }

            PriceLevel& level = best_bid_it->second;

            while (aggressive_order->remaining_quantity > 0 && !level.empty()) {
                OrderPtr resting_order = level.frontOrder();
                if (!resting_order) {
                    level.popFront();
                    continue;
                }

                uint64_t match_qty = std::min(aggressive_order->remaining_quantity, resting_order->remaining_quantity);
                double match_price = resting_order->price; // Resting order sets the price

                aggressive_order->remaining_quantity -= match_qty;
                resting_order->remaining_quantity -= match_qty;
                level.reduceQuantity(match_qty);

                if (aggressive_order->remaining_quantity == 0) {
                    aggressive_order->status = OrderStatus::Filled;
                } else {
                    aggressive_order->status = OrderStatus::PartiallyFilled;
                }

                if (resting_order->remaining_quantity == 0) {
                    resting_order->status = OrderStatus::Filled;
                } else {
                    resting_order->status = OrderStatus::PartiallyFilled;
                }

                trades.emplace_back(
                    next_trade_id++,
                    resting_order->order_id,
                    aggressive_order->order_id,
                    symbol_,
                    match_price,
                    match_qty,
                    aggressive_order->timestamp,
                    Side::Sell
                );

                if (resting_order->isFilled()) {
                    order_lookup_.erase(resting_order->order_id);
                    level.popFront();
                }
            }

            if (level.empty()) {
                bids_.erase(best_bid_it);
            }
        }
    }

    return trades;
}

void OrderBook::addRestingOrder(const OrderPtr& order) {
    if (!order || order->remaining_quantity == 0) return;

    if (order->side == Side::Buy) {
        auto [it, _] = bids_.try_emplace(order->price, PriceLevel(order->price));
        auto order_it = it->second.addOrder(order);
        order_lookup_[order->order_id] = OrderLocation{order, Side::Buy, order->price, order_it};
    } else {
        auto [it, _] = asks_.try_emplace(order->price, PriceLevel(order->price));
        auto order_it = it->second.addOrder(order);
        order_lookup_[order->order_id] = OrderLocation{order, Side::Sell, order->price, order_it};
    }
}

bool OrderBook::cancelOrder(uint64_t order_id) {
    auto it = order_lookup_.find(order_id);
    if (it == order_lookup_.end()) {
        return false;
    }

    OrderLocation& loc = it->second;
    OrderPtr order = loc.order;

    if (loc.side == Side::Buy) {
        auto level_it = bids_.find(loc.price);
        if (level_it != bids_.end()) {
            level_it->second.removeOrder(loc.it);
            if (level_it->second.empty()) {
                bids_.erase(level_it);
            }
        }
    } else {
        auto level_it = asks_.find(loc.price);
        if (level_it != asks_.end()) {
            level_it->second.removeOrder(loc.it);
            if (level_it->second.empty()) {
                asks_.erase(level_it);
            }
        }
    }

    order->status = OrderStatus::Cancelled;
    order_lookup_.erase(it);
    return true;
}

bool OrderBook::modifyOrder(uint64_t order_id, double new_price, uint64_t new_qty, uint64_t timestamp, uint64_t& next_trade_id, std::vector<Trade>& generated_trades) {
    auto it = order_lookup_.find(order_id);
    if (it == order_lookup_.end()) {
        return false;
    }

    OrderLocation loc = it->second;
    OrderPtr old_order = loc.order;

    if (old_order->isTerminal()) {
        return false;
    }

    // In-place reduction if price is unchanged and new quantity is <= remaining quantity
    if (std::abs(old_order->price - new_price) < 1e-9 && new_qty <= old_order->remaining_quantity) {
        uint64_t reduction = old_order->remaining_quantity - new_qty;
        old_order->remaining_quantity = new_qty;
        old_order->quantity = old_order->filledQuantity() + new_qty;

        if (loc.side == Side::Buy) {
            auto level_it = bids_.find(loc.price);
            if (level_it != bids_.end()) {
                level_it->second.reduceQuantity(reduction);
            }
        } else {
            auto level_it = asks_.find(loc.price);
            if (level_it != asks_.end()) {
                level_it->second.reduceQuantity(reduction);
            }
        }

        if (new_qty == 0) {
            cancelOrder(order_id);
        }
        return true;
    }

    // Otherwise, cancel and re-submit to lose priority
    Side side = old_order->side;
    cancelOrder(order_id);

    auto new_order = std::make_shared<Order>(
        order_id,
        symbol_,
        side,
        OrderType::Limit,
        new_price,
        new_qty,
        timestamp
    );

    generated_trades = addOrder(new_order, next_trade_id);
    return true;
}

std::optional<LevelView> OrderBook::bestBid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->second.toView();
}

std::optional<LevelView> OrderBook::bestAsk() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->second.toView();
}

BBO OrderBook::getBBO() const {
    BBO bbo;
    bbo.best_bid = bestBid();
    bbo.best_ask = bestAsk();
    return bbo;
}

std::vector<LevelView> OrderBook::getDepth(Side side, size_t levels) const {
    std::vector<LevelView> depth;
    depth.reserve(levels);

    if (side == Side::Buy) {
        size_t count = 0;
        for (const auto& [_, level] : bids_) {
            if (count++ >= levels) break;
            depth.push_back(level.toView());
        }
    } else {
        size_t count = 0;
        for (const auto& [_, level] : asks_) {
            if (count++ >= levels) break;
            depth.push_back(level.toView());
        }
    }

    return depth;
}

OrderPtr OrderBook::getOrder(uint64_t order_id) const {
    auto it = order_history_.find(order_id);
    if (it != order_history_.end()) {
        return it->second;
    }
    return nullptr;
}

std::optional<OrderStatus> OrderBook::getOrderStatus(uint64_t order_id) const {
    auto it = order_history_.find(order_id);
    if (it != order_history_.end()) {
        return it->second->status;
    }
    return std::nullopt;
}

uint64_t OrderBook::totalBidVolume() const noexcept {
    uint64_t total = 0;
    for (const auto& [_, level] : bids_) {
        total += level.totalQuantity();
    }
    return total;
}

uint64_t OrderBook::totalAskVolume() const noexcept {
    uint64_t total = 0;
    for (const auto& [_, level] : asks_) {
        total += level.totalQuantity();
    }
    return total;
}

void OrderBook::clear() {
    bids_.clear();
    asks_.clear();
    order_lookup_.clear();
    order_history_.clear();
}

} // namespace lobster
