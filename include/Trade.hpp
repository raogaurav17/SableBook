#pragma once

#include "Types.hpp"
#include <cstdint>
#include <string>

namespace sablebook {

struct Trade {
    uint64_t    trade_id{0};
    uint64_t    buy_order_id{0};
    uint64_t    sell_order_id{0};
    std::string symbol{"DEFAULT"};
    double      price{0.0};
    uint64_t    quantity{0};
    uint64_t    timestamp{0};
    Side        aggressor_side{Side::Buy};

    Trade() = default;

    Trade(uint64_t tid,
          uint64_t b_id,
          uint64_t s_id,
          std::string sym,
          double p,
          uint64_t qty,
          uint64_t ts,
          Side agg_side)
        : trade_id(tid),
          buy_order_id(b_id),
          sell_order_id(s_id),
          symbol(std::move(sym)),
          price(p),
          quantity(qty),
          timestamp(ts),
          aggressor_side(agg_side) {}
};

} // namespace sablebook
