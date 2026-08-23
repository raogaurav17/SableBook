#pragma once

#include "Types.hpp"
#include <cstdint>
#include <string>
#include <memory>

namespace sablebook {

struct Order {
    uint64_t     order_id{0};
    std::string  symbol{"DEFAULT"};
    Side         side{Side::Buy};
    OrderType    type{OrderType::Limit};
    double       price{0.0};
    uint64_t     quantity{0};
    uint64_t     remaining_quantity{0};
    uint64_t     timestamp{0};
    OrderStatus  status{OrderStatus::New};

    Order() = default;

    Order(uint64_t id,
          std::string sym,
          Side s,
          OrderType t,
          double p,
          uint64_t qty,
          uint64_t ts = 0)
        : order_id(id),
          symbol(std::move(sym)),
          side(s),
          type(t),
          price(p),
          quantity(qty),
          remaining_quantity(qty),
          timestamp(ts),
          status(OrderStatus::New) {}

    [[nodiscard]] uint64_t filledQuantity() const noexcept {
        return quantity - remaining_quantity;
    }

    [[nodiscard]] bool isFilled() const noexcept {
        return remaining_quantity == 0;
    }

    [[nodiscard]] bool isTerminal() const noexcept {
        return status == OrderStatus::Filled ||
               status == OrderStatus::Cancelled ||
               status == OrderStatus::Rejected;
    }
};

using OrderPtr = std::shared_ptr<Order>;

} // namespace sablebook
