#pragma once

#include "Order.hpp"
#include "Types.hpp"
#include <list>
#include <cstdint>
#include <cstddef>

namespace lobster {

class PriceLevel {
public:
    using OrderList = std::list<OrderPtr>;
    using OrderIterator = OrderList::iterator;

    explicit PriceLevel(double price = 0.0)
        : price_(price), total_quantity_(0) {}

    [[nodiscard]] double price() const noexcept { return price_; }
    [[nodiscard]] uint64_t totalQuantity() const noexcept { return total_quantity_; }
    [[nodiscard]] size_t orderCount() const noexcept { return orders_.size(); }
    [[nodiscard]] bool empty() const noexcept { return orders_.empty(); }

    OrderIterator addOrder(const OrderPtr& order) {
        orders_.push_back(order);
        total_quantity_ += order->remaining_quantity;
        return std::prev(orders_.end());
    }

    void removeOrder(OrderIterator it) {
        if (it != orders_.end() && *it) {
            total_quantity_ -= (*it)->remaining_quantity;
            orders_.erase(it);
        }
    }

    void reduceQuantity(uint64_t qty) noexcept {
        if (qty >= total_quantity_) {
            total_quantity_ = 0;
        } else {
            total_quantity_ -= qty;
        }
    }

    [[nodiscard]] OrderPtr frontOrder() const {
        if (orders_.empty()) return nullptr;
        return orders_.front();
    }

    void popFront() {
        if (!orders_.empty()) {
            total_quantity_ -= orders_.front()->remaining_quantity;
            orders_.pop_front();
        }
    }

    [[nodiscard]] const OrderList& orders() const noexcept {
        return orders_;
    }

    [[nodiscard]] LevelView toView() const noexcept {
        return LevelView{price_, total_quantity_, orders_.size()};
    }

private:
    double price_{0.0};
    uint64_t total_quantity_{0};
    OrderList orders_;
};

} // namespace lobster
