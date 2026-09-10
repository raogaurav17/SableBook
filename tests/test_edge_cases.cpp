#include "test_framework.hpp"
#include "MatchingEngine.hpp"
#include "Metrics.hpp"

#include <limits>

using namespace sablebook;

TEST_CASE(TestMetrics_InterpolatedPercentiles) {
    LatencyTracker tracker;
    tracker.record(1);
    tracker.record(2);
    tracker.record(3);
    tracker.record(4);

    auto stats = tracker.getStats();
    ASSERT_EQ(stats.count, 4);
    ASSERT_DOUBLE_EQ(stats.p50_ns, 2.5);
    ASSERT_DOUBLE_EQ(stats.p90_ns, 3.7);
    ASSERT_DOUBLE_EQ(stats.p99_ns, 3.97);
    ASSERT_DOUBLE_EQ(stats.p99_9_ns, 3.997);
}

TEST_CASE(TestEdgeCases_ValidationAndRejections) {
    MatchingEngine engine(100'000); // max size 100,000

    RejectReason reason = RejectReason::None;
    uint64_t rejected_id = 0;
    RejectReason cb_reason = RejectReason::None;
    engine.setRejectCallback([&](uint64_t id, RejectReason r) {
        rejected_id = id;
        cb_reason = r;
    });

    // Zero quantity
    uint64_t o1 = engine.submitOrder("DEFAULT", Side::Buy, OrderType::Limit, 100.0, 0, &reason);
    ASSERT_EQ(o1, 0);
    ASSERT_EQ(reason, RejectReason::InvalidQuantity);
    ASSERT_EQ(cb_reason, RejectReason::InvalidQuantity);

    // Negative price
    uint64_t o2 = engine.submitOrder("DEFAULT", Side::Buy, OrderType::Limit, -50.0, 10, &reason);
    ASSERT_EQ(o2, 0);
    ASSERT_EQ(reason, RejectReason::InvalidPrice);

    // Zero price limit
    uint64_t o3 = engine.submitOrder("DEFAULT", Side::Buy, OrderType::Limit, 0.0, 10, &reason);
    ASSERT_EQ(o3, 0);
    ASSERT_EQ(reason, RejectReason::InvalidPrice);

    // Non-finite prices
    uint64_t o_nan = engine.submitOrder(
        "DEFAULT", Side::Buy, OrderType::Limit,
        std::numeric_limits<double>::quiet_NaN(), 10, &reason);
    ASSERT_EQ(o_nan, 0);
    ASSERT_EQ(reason, RejectReason::InvalidPrice);

    uint64_t o_inf = engine.submitOrder(
        "DEFAULT", Side::Buy, OrderType::Limit,
        std::numeric_limits<double>::infinity(), 10, &reason);
    ASSERT_EQ(o_inf, 0);
    ASSERT_EQ(reason, RejectReason::InvalidPrice);

    // Exceeds max order size
    uint64_t o4 = engine.submitOrder("DEFAULT", Side::Buy, OrderType::Limit, 100.0, 200'000, &reason);
    ASSERT_EQ(o4, 0);
    ASSERT_EQ(reason, RejectReason::MaxOrderSizeExceeded);
}

TEST_CASE(TestEdgeCases_OrderModification) {
    MatchingEngine engine;
    std::vector<Trade> trades;
    engine.setTradeCallback([&](const Trade& t) {
        trades.push_back(t);
    });

    // Place two bids at 100.0
    uint64_t b1 = engine.submitLimitOrder(Side::Buy, 100.0, 20); // earlier
    uint64_t b2 = engine.submitLimitOrder(Side::Buy, 100.0, 30); // later

    // Reduce b1 quantity to 10 (keeps time priority)
    ASSERT_TRUE(engine.modifyOrder(b1, 100.0, 10));
    ASSERT_EQ(engine.getBBO().best_bid->total_quantity, 40);

    // Aggressive sell for 10 units should match b1
    engine.submitLimitOrder(Side::Sell, 100.0, 10);
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].buy_order_id, b1);
    ASSERT_EQ(trades[0].quantity, 10);
    ASSERT_EQ(engine.getOrderStatus(b1), OrderStatus::Filled);

    ASSERT_FALSE(engine.modifyOrder(b2, std::numeric_limits<double>::quiet_NaN(), 10));
    ASSERT_FALSE(engine.modifyOrder(b2, std::numeric_limits<double>::infinity(), 10));

    // Modify b2 to new price 101.0 (loses old position, moves to new level)
    ASSERT_TRUE(engine.modifyOrder(b2, 101.0, 25));
    BBO bbo = engine.getBBO();
    ASSERT_TRUE(bbo.hasBid());
    ASSERT_DOUBLE_EQ(bbo.best_bid->price, 101.0);
    ASSERT_EQ(bbo.best_bid->total_quantity, 25);
}

TEST_CASE(TestEdgeCases_PartialFillModificationPreservesFillHistory) {
    MatchingEngine engine;

    uint64_t sell_id = engine.submitLimitOrder(Side::Sell, 100.0, 10);
    uint64_t buy_id = engine.submitLimitOrder(Side::Buy, 100.0, 20);

    ASSERT_EQ(engine.getOrderStatus(sell_id), OrderStatus::Filled);
    ASSERT_EQ(engine.getOrderStatus(buy_id), OrderStatus::PartiallyFilled);

    OrderPtr before_modify = engine.getOrder(buy_id);
    ASSERT_TRUE(before_modify != nullptr);
    ASSERT_EQ(before_modify->quantity, 20);
    ASSERT_EQ(before_modify->remaining_quantity, 10);
    ASSERT_EQ(before_modify->filledQuantity(), 10);

    ASSERT_TRUE(engine.modifyOrder(buy_id, 99.0, 5));

    OrderPtr amended = engine.getOrder(buy_id);
    ASSERT_TRUE(amended != nullptr);
    ASSERT_EQ(amended->quantity, 15);
    ASSERT_EQ(amended->remaining_quantity, 5);
    ASSERT_EQ(amended->filledQuantity(), 10);
    ASSERT_EQ(engine.getOrderStatus(buy_id), OrderStatus::PartiallyFilled);
    ASSERT_TRUE(engine.getBBO().hasBid());
    ASSERT_DOUBLE_EQ(engine.getBBO().best_bid->price, 99.0);
    ASSERT_EQ(engine.getBBO().best_bid->total_quantity, 5);
}

TEST_CASE(TestEdgeCases_MultiInstrument) {
    MatchingEngine engine;

    engine.registerSymbol("AAPL");
    engine.registerSymbol("MSFT");

    engine.submitOrder("AAPL", Side::Buy, OrderType::Limit, 150.0, 100);
    engine.submitOrder("MSFT", Side::Buy, OrderType::Limit, 300.0, 50);

    BBO aapl_bbo = engine.getBBO("AAPL");
    BBO msft_bbo = engine.getBBO("MSFT");

    ASSERT_TRUE(aapl_bbo.hasBid());
    ASSERT_DOUBLE_EQ(aapl_bbo.best_bid->price, 150.0);
    ASSERT_EQ(aapl_bbo.best_bid->total_quantity, 100);

    ASSERT_TRUE(msft_bbo.hasBid());
    ASSERT_DOUBLE_EQ(msft_bbo.best_bid->price, 300.0);
    ASSERT_EQ(msft_bbo.best_bid->total_quantity, 50);
}

TEST_CASE(TestEdgeCases_BookUpdateCallbacks) {
    MatchingEngine engine;
    int update_count = 0;
    std::string last_symbol;
    BBO last_bbo;

    engine.setBookUpdateCallback([&](const std::string& sym, const BBO& bbo) {
        update_count++;
        last_symbol = sym;
        last_bbo = bbo;
    });

    engine.submitLimitOrder(Side::Buy, 100.0, 10, "GOOG");
    ASSERT_EQ(update_count, 1);
    ASSERT_EQ(last_symbol, "GOOG");
    ASSERT_TRUE(last_bbo.hasBid());
    ASSERT_DOUBLE_EQ(last_bbo.best_bid->price, 100.0);

    engine.submitLimitOrder(Side::Sell, 105.0, 20, "GOOG");
    ASSERT_EQ(update_count, 2);
    ASSERT_TRUE(last_bbo.hasAsk());
    ASSERT_DOUBLE_EQ(last_bbo.best_ask->price, 105.0);
}

TEST_CASE(TestEdgeCases_TradeCallbackCanResetDuringSubmit) {
    MatchingEngine engine;
    bool callback_called = false;
    engine.setTradeCallback([&](const Trade&) {
        callback_called = true;
        engine.reset();
    });

    engine.submitLimitOrder(Side::Sell, 100.0, 10);
    engine.submitLimitOrder(Side::Buy, 100.0, 10);

    ASSERT_TRUE(callback_called);
    ASSERT_FALSE(engine.getBBO().hasBid());
    ASSERT_FALSE(engine.getBBO().hasAsk());
}

TEST_CASE(TestEdgeCases_TradeCallbackCanResetDuringModify) {
    MatchingEngine engine;
    uint64_t sell_id = engine.submitLimitOrder(Side::Sell, 100.0, 10);
    uint64_t buy_id = engine.submitLimitOrder(Side::Buy, 99.0, 20);
    bool callback_called = false;

    engine.setTradeCallback([&](const Trade&) {
        callback_called = true;
        engine.reset();
    });

    ASSERT_TRUE(engine.modifyOrder(buy_id, 101.0, 20));
    ASSERT_TRUE(callback_called);
    ASSERT_FALSE(engine.getBBO().hasBid());
    ASSERT_FALSE(engine.getBBO().hasAsk());
    ASSERT_EQ(engine.getOrderStatus(sell_id), std::nullopt);
}
