#include "MatchingEngine.hpp"

#include <cmath>

namespace sablebook {

MatchingEngine::MatchingEngine(uint64_t max_order_quantity)
    : max_order_quantity_(max_order_quantity) {
    registerSymbol("DEFAULT");
}

void MatchingEngine::registerSymbol(const std::string& symbol) {
    if (books_.find(symbol) == books_.end()) {
        books_[symbol] = std::make_unique<OrderBook>(symbol);
    }
}

bool MatchingEngine::hasSymbol(const std::string& symbol) const {
    return books_.find(symbol) != books_.end();
}

OrderBook* MatchingEngine::getOrCreateBook(const std::string& symbol) {
    auto it = books_.find(symbol);
    if (it == books_.end()) {
        auto [inserted_it, _] = books_.try_emplace(symbol, std::make_unique<OrderBook>(symbol));
        return inserted_it->second.get();
    }
    return it->second.get();
}

uint64_t MatchingEngine::submitOrder(const std::string& symbol,
                                     Side side,
                                     OrderType type,
                                     double price,
                                     uint64_t quantity,
                                     RejectReason* reject_reason) {
    if (quantity == 0) {
        if (reject_reason) *reject_reason = RejectReason::InvalidQuantity;
        if (on_reject_) on_reject_(0, RejectReason::InvalidQuantity);
        return 0;
    }

    if (quantity > max_order_quantity_) {
        if (reject_reason) *reject_reason = RejectReason::MaxOrderSizeExceeded;
        if (on_reject_) on_reject_(0, RejectReason::MaxOrderSizeExceeded);
        return 0;
    }

    if (type == OrderType::Limit && (!std::isfinite(price) || price <= 0.0)) {
        if (reject_reason) *reject_reason = RejectReason::InvalidPrice;
        if (on_reject_) on_reject_(0, RejectReason::InvalidPrice);
        return 0;
    }

    if (reject_reason) *reject_reason = RejectReason::None;

    uint64_t order_id = next_order_id_++;
    uint64_t start_ts = getTimestampNs();

    auto order = std::make_shared<Order>(
        order_id,
        symbol,
        side,
        type,
        price,
        quantity,
        start_ts
    );

    OrderBook* book = getOrCreateBook(symbol);
    order_symbol_map_[order_id] = symbol;

    std::vector<Trade> trades = book->addOrder(order, next_trade_id_);

    uint64_t end_ts = getTimestampNs();
    latency_tracker_.record(end_ts - start_ts);

    if (order->isTerminal()) {
        terminal_orders_[order_id] = order;
    }

    if (on_trade_) {
        for (const auto& trade : trades) {
            on_trade_(trade);
        }
    }

    if (on_book_update_) {
        on_book_update_(symbol, book->getBBO());
    }

    return order_id;
}

uint64_t MatchingEngine::submitLimitOrder(Side side, double price, uint64_t quantity, const std::string& symbol) {
    return submitOrder(symbol, side, OrderType::Limit, price, quantity);
}

uint64_t MatchingEngine::submitMarketOrder(Side side, uint64_t quantity, const std::string& symbol) {
    return submitOrder(symbol, side, OrderType::Market, 0.0, quantity);
}

bool MatchingEngine::cancelOrder(uint64_t order_id) {
    auto sym_it = order_symbol_map_.find(order_id);
    if (sym_it == order_symbol_map_.end()) {
        return false;
    }

    OrderBook* book = getOrCreateBook(sym_it->second);
    OrderPtr order = book->getOrder(order_id);

    uint64_t start_ts = getTimestampNs();
    bool success = book->cancelOrder(order_id);
    uint64_t end_ts = getTimestampNs();
    latency_tracker_.record(end_ts - start_ts);

    if (success) {
        if (order) {
            terminal_orders_[order_id] = order;
        }
        if (on_book_update_) {
            on_book_update_(sym_it->second, book->getBBO());
        }
    }

    return success;
}

bool MatchingEngine::modifyOrder(uint64_t order_id, double new_price, uint64_t new_qty) {
    if (new_qty == 0 || !std::isfinite(new_price) || new_price <= 0.0) {
        return false;
    }

    auto sym_it = order_symbol_map_.find(order_id);
    if (sym_it == order_symbol_map_.end()) {
        return false;
    }

    OrderBook* book = getOrCreateBook(sym_it->second);
    uint64_t ts = getTimestampNs();
    std::vector<Trade> trades;

    bool success = book->modifyOrder(order_id, new_price, new_qty, ts, next_trade_id_, trades);

    if (success) {
        if (on_trade_) {
            for (const auto& trade : trades) {
                on_trade_(trade);
            }
        }
        if (on_book_update_) {
            on_book_update_(sym_it->second, book->getBBO());
        }
    }

    return success;
}

BBO MatchingEngine::getBBO(const std::string& symbol) const {
    auto it = books_.find(symbol);
    if (it != books_.end()) {
        return it->second->getBBO();
    }
    return {};
}

std::vector<LevelView> MatchingEngine::getDepth(Side side, size_t levels, const std::string& symbol) const {
    auto it = books_.find(symbol);
    if (it != books_.end()) {
        return it->second->getDepth(side, levels);
    }
    return {};
}

std::optional<OrderStatus> MatchingEngine::getOrderStatus(uint64_t order_id) const {
    auto sym_it = order_symbol_map_.find(order_id);
    if (sym_it != order_symbol_map_.end()) {
        auto book_it = books_.find(sym_it->second);
        if (book_it != books_.end()) {
            return book_it->second->getOrderStatus(order_id);
        }
    }

    auto term_it = terminal_orders_.find(order_id);
    if (term_it != terminal_orders_.end()) {
        return term_it->second->status;
    }

    return std::nullopt;
}

OrderPtr MatchingEngine::getOrder(uint64_t order_id) const {
    auto sym_it = order_symbol_map_.find(order_id);
    if (sym_it != order_symbol_map_.end()) {
        auto book_it = books_.find(sym_it->second);
        if (book_it != books_.end()) {
            return book_it->second->getOrder(order_id);
        }
    }

    auto term_it = terminal_orders_.find(order_id);
    if (term_it != terminal_orders_.end()) {
        return term_it->second;
    }

    return nullptr;
}

const OrderBook* MatchingEngine::getOrderBook(const std::string& symbol) const {
    auto it = books_.find(symbol);
    if (it != books_.end()) {
        return it->second.get();
    }
    return nullptr;
}

OrderBook* MatchingEngine::getOrderBook(const std::string& symbol) {
    return getOrCreateBook(symbol);
}

void MatchingEngine::reset() {
    books_.clear();
    order_symbol_map_.clear();
    terminal_orders_.clear();
    next_order_id_ = 1;
    next_trade_id_ = 1;
    latency_tracker_.clear();
    registerSymbol("DEFAULT");
}

} // namespace sablebook
