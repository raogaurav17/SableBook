#include "test_framework.hpp"
#include "MatchingEngine.hpp"

using namespace sablebook;

TEST_CASE(TestCancel_Basic) {
    MatchingEngine engine;

    uint64_t o1 = engine.submitLimitOrder(Side::Buy, 100.0, 10);
    ASSERT_TRUE(engine.getBBO().hasBid());

    bool cancelled = engine.cancelOrder(o1);
    ASSERT_TRUE(cancelled);
    ASSERT_EQ(engine.getOrderStatus(o1), OrderStatus::Cancelled);
    ASSERT_FALSE(engine.getBBO().hasBid());
}

TEST_CASE(TestCancel_QueuePositions_HeadMiddleTail) {
    MatchingEngine engine;

    // Place 3 bids at the same price 100.0
    uint64_t o1 = engine.submitLimitOrder(Side::Buy, 100.0, 10);
    uint64_t o2 = engine.submitLimitOrder(Side::Buy, 100.0, 20);
    uint64_t o3 = engine.submitLimitOrder(Side::Buy, 100.0, 30);

    ASSERT_EQ(engine.getBBO().best_bid->total_quantity, 60);

    // Cancel Middle (o2)
    ASSERT_TRUE(engine.cancelOrder(o2));
    ASSERT_EQ(engine.getBBO().best_bid->total_quantity, 40);

    // Cancel Head (o1)
    ASSERT_TRUE(engine.cancelOrder(o1));
    ASSERT_EQ(engine.getBBO().best_bid->total_quantity, 30);

    // Cancel Tail (o3)
    ASSERT_TRUE(engine.cancelOrder(o3));
    ASSERT_FALSE(engine.getBBO().hasBid());
}

TEST_CASE(TestCancel_NonExistentAndAlreadyFilled) {
    MatchingEngine engine;

    // Non-existent ID
    ASSERT_FALSE(engine.cancelOrder(99999));

    // Filled order
    uint64_t s1 = engine.submitLimitOrder(Side::Sell, 100.0, 10);
    uint64_t b1 = engine.submitLimitOrder(Side::Buy, 100.0, 10);
    ASSERT_EQ(engine.getOrderStatus(s1), OrderStatus::Filled);
    ASSERT_EQ(engine.getOrderStatus(b1), OrderStatus::Filled);

    // Cancelling a filled order should return false
    ASSERT_FALSE(engine.cancelOrder(s1));
    ASSERT_FALSE(engine.cancelOrder(b1));
}

TEST_CASE(TestCancel_DoubleCancel) {
    MatchingEngine engine;

    uint64_t o1 = engine.submitLimitOrder(Side::Buy, 50.0, 10);
    ASSERT_TRUE(engine.cancelOrder(o1));
    ASSERT_FALSE(engine.cancelOrder(o1));
}
