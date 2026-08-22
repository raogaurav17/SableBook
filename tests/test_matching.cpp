#include "test_framework.hpp"
#include "MatchingEngine.hpp"

using namespace lobster;

TEST_CASE(TestMatching_ExactLimitMatch) {
    MatchingEngine engine;

    // Resting Sell: 10 units @ 100.0
    uint64_t s_id = engine.submitLimitOrder(Side::Sell, 100.0, 10);
    // Aggressive Buy: 10 units @ 100.0
    uint64_t b_id = engine.submitLimitOrder(Side::Buy, 100.0, 10);

    ASSERT_EQ(engine.totalTradesGenerated(), 1);
    ASSERT_EQ(engine.getOrderStatus(s_id), OrderStatus::Filled);
    ASSERT_EQ(engine.getOrderStatus(b_id), OrderStatus::Filled);

    BBO bbo = engine.getBBO();
    ASSERT_FALSE(bbo.hasBid());
    ASSERT_FALSE(bbo.hasAsk());
}

TEST_CASE(TestMatching_RestingOrderSetsPrice) {
    MatchingEngine engine;
    std::vector<Trade> trades;
    engine.setTradeCallback([&](const Trade& t) {
        trades.push_back(t);
    });

    // Resting Bid @ 105.0
    engine.submitLimitOrder(Side::Buy, 105.0, 10);
    // Aggressive Sell @ 100.0 (willing to sell cheaper)
    engine.submitLimitOrder(Side::Sell, 100.0, 10);

    ASSERT_EQ(trades.size(), 1);
    // Trade should execute at 105.0 (resting order price)
    ASSERT_DOUBLE_EQ(trades[0].price, 105.0);
    ASSERT_EQ(trades[0].quantity, 10);
}

TEST_CASE(TestMatching_PartialFillResting) {
    MatchingEngine engine;

    // Resting Ask: 100 units @ 50.0
    uint64_t s_id = engine.submitLimitOrder(Side::Sell, 50.0, 100);
    // Aggressive Buy: 40 units @ 50.0
    uint64_t b_id = engine.submitLimitOrder(Side::Buy, 50.0, 40);

    ASSERT_EQ(engine.totalTradesGenerated(), 1);
    ASSERT_EQ(engine.getOrderStatus(b_id), OrderStatus::Filled);
    ASSERT_EQ(engine.getOrderStatus(s_id), OrderStatus::PartiallyFilled);

    OrderPtr resting = engine.getOrder(s_id);
    ASSERT_TRUE(resting != nullptr);
    ASSERT_EQ(resting->remaining_quantity, 60);
    ASSERT_EQ(resting->filledQuantity(), 40);

    BBO bbo = engine.getBBO();
    ASSERT_TRUE(bbo.hasAsk());
    ASSERT_EQ(bbo.best_ask->total_quantity, 60);
}

TEST_CASE(TestMatching_PartialFillAggressiveRestsOnBook) {
    MatchingEngine engine;

    // Resting Ask: 30 units @ 50.0
    uint64_t s_id = engine.submitLimitOrder(Side::Sell, 50.0, 30);
    // Aggressive Buy: 100 units @ 50.0
    uint64_t b_id = engine.submitLimitOrder(Side::Buy, 50.0, 100);

    ASSERT_EQ(engine.totalTradesGenerated(), 1);
    ASSERT_EQ(engine.getOrderStatus(s_id), OrderStatus::Filled);
    ASSERT_EQ(engine.getOrderStatus(b_id), OrderStatus::PartiallyFilled);

    // Remaining 70 units should now rest on the Buy side
    BBO bbo = engine.getBBO();
    ASSERT_FALSE(bbo.hasAsk());
    ASSERT_TRUE(bbo.hasBid());
    ASSERT_DOUBLE_EQ(bbo.best_bid->price, 50.0);
    ASSERT_EQ(bbo.best_bid->total_quantity, 70);
}

TEST_CASE(TestMatching_MultiLevelSweep) {
    MatchingEngine engine;
    std::vector<Trade> trades;
    engine.setTradeCallback([&](const Trade& t) {
        trades.push_back(t);
    });

    // Resting Asks across 3 levels
    engine.submitLimitOrder(Side::Sell, 101.0, 10);
    engine.submitLimitOrder(Side::Sell, 102.0, 20);
    engine.submitLimitOrder(Side::Sell, 103.0, 30);

    // Aggressive Buy for 50 units @ 105.0
    uint64_t b_id = engine.submitLimitOrder(Side::Buy, 105.0, 50);

    ASSERT_EQ(trades.size(), 3);
    // Level 1: 10 @ 101.0
    ASSERT_DOUBLE_EQ(trades[0].price, 101.0);
    ASSERT_EQ(trades[0].quantity, 10);
    // Level 2: 20 @ 102.0
    ASSERT_DOUBLE_EQ(trades[1].price, 102.0);
    ASSERT_EQ(trades[1].quantity, 20);
    // Level 3: 20 @ 103.0 (leaving 10 resting @ 103.0)
    ASSERT_DOUBLE_EQ(trades[2].price, 103.0);
    ASSERT_EQ(trades[2].quantity, 20);

    ASSERT_EQ(engine.getOrderStatus(b_id), OrderStatus::Filled);

    BBO bbo = engine.getBBO();
    ASSERT_TRUE(bbo.hasAsk());
    ASSERT_DOUBLE_EQ(bbo.best_ask->price, 103.0);
    ASSERT_EQ(bbo.best_ask->total_quantity, 10);
}

TEST_CASE(TestMatching_PriceTimePriorityFIFO) {
    MatchingEngine engine;
    std::vector<Trade> trades;
    engine.setTradeCallback([&](const Trade& t) {
        trades.push_back(t);
    });

    // Two resting Asks at same price 100.0
    uint64_t s1 = engine.submitLimitOrder(Side::Sell, 100.0, 10); // earlier
    uint64_t s2 = engine.submitLimitOrder(Side::Sell, 100.0, 15); // later

    // Aggressive Buy for 12 units
    engine.submitLimitOrder(Side::Buy, 100.0, 12);

    ASSERT_EQ(trades.size(), 2);
    // First trade must match s1 completely (10 units)
    ASSERT_EQ(trades[0].sell_order_id, s1);
    ASSERT_EQ(trades[0].quantity, 10);
    ASSERT_EQ(engine.getOrderStatus(s1), OrderStatus::Filled);

    // Second trade must match s2 partially (2 units)
    ASSERT_EQ(trades[1].sell_order_id, s2);
    ASSERT_EQ(trades[1].quantity, 2);
    ASSERT_EQ(engine.getOrderStatus(s2), OrderStatus::PartiallyFilled);
}

TEST_CASE(TestMatching_MarketOrders) {
    MatchingEngine engine;
    std::vector<Trade> trades;
    engine.setTradeCallback([&](const Trade& t) {
        trades.push_back(t);
    });

    engine.submitLimitOrder(Side::Sell, 200.0, 15);
    engine.submitLimitOrder(Side::Sell, 205.0, 25);

    // Market Buy for 30 units
    uint64_t mb_id = engine.submitMarketOrder(Side::Buy, 30);

    ASSERT_EQ(trades.size(), 2);
    ASSERT_DOUBLE_EQ(trades[0].price, 200.0);
    ASSERT_EQ(trades[0].quantity, 15);
    ASSERT_DOUBLE_EQ(trades[1].price, 205.0);
    ASSERT_EQ(trades[1].quantity, 15);
    ASSERT_EQ(engine.getOrderStatus(mb_id), OrderStatus::Filled);

    // Market Order with insufficient liquidity: remaining discarded
    uint64_t mb2_id = engine.submitMarketOrder(Side::Buy, 50);
    ASSERT_EQ(trades.size(), 3);
    ASSERT_DOUBLE_EQ(trades[2].price, 205.0);
    ASSERT_EQ(trades[2].quantity, 10);
    // Unfilled remaining 40 discarded
    ASSERT_EQ(engine.getOrderStatus(mb2_id), OrderStatus::PartiallyFilled);
    ASSERT_FALSE(engine.getBBO().hasAsk());
}
