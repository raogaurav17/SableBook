#include "test_framework.hpp"
#include "OrderBook.hpp"

using namespace sablebook;

TEST_CASE(TestOrderBook_EmptyBook) {
    OrderBook book("BTC-USD");
    ASSERT_EQ(book.symbol(), "BTC-USD");
    ASSERT_FALSE(book.bestBid().has_value());
    ASSERT_FALSE(book.bestAsk().has_value());
    ASSERT_EQ(book.totalBidVolume(), 0);
    ASSERT_EQ(book.totalAskVolume(), 0);
    ASSERT_EQ(book.bidLevelCount(), 0);
    ASSERT_EQ(book.askLevelCount(), 0);
}

TEST_CASE(TestOrderBook_AddRestingOrdersAndBBO) {
    OrderBook book("BTC-USD");
    uint64_t next_tid = 1;

    auto bid1 = std::make_shared<Order>(1, "BTC-USD", Side::Buy, OrderType::Limit, 100.0, 10, 1000);
    auto bid2 = std::make_shared<Order>(2, "BTC-USD", Side::Buy, OrderType::Limit, 101.0, 15, 1001);
    auto bid3 = std::make_shared<Order>(3, "BTC-USD", Side::Buy, OrderType::Limit, 100.0, 5, 1002);

    auto ask1 = std::make_shared<Order>(4, "BTC-USD", Side::Sell, OrderType::Limit, 105.0, 20, 1003);
    auto ask2 = std::make_shared<Order>(5, "BTC-USD", Side::Sell, OrderType::Limit, 103.0, 8, 1004);

    book.addOrder(bid1, next_tid);
    book.addOrder(bid2, next_tid);
    book.addOrder(bid3, next_tid);
    book.addOrder(ask1, next_tid);
    book.addOrder(ask2, next_tid);

    ASSERT_EQ(book.bidLevelCount(), 2);
    ASSERT_EQ(book.askLevelCount(), 2);
    ASSERT_EQ(book.totalBidVolume(), 30);
    ASSERT_EQ(book.totalAskVolume(), 28);

    BBO bbo = book.getBBO();
    ASSERT_TRUE(bbo.hasBid());
    ASSERT_TRUE(bbo.hasAsk());
    ASSERT_DOUBLE_EQ(bbo.best_bid->price, 101.0);
    ASSERT_EQ(bbo.best_bid->total_quantity, 15);
    ASSERT_DOUBLE_EQ(bbo.best_ask->price, 103.0);
    ASSERT_EQ(bbo.best_ask->total_quantity, 8);

    ASSERT_TRUE(bbo.spread().has_value());
    ASSERT_DOUBLE_EQ(*bbo.spread(), 2.0);
    ASSERT_TRUE(bbo.midPrice().has_value());
    ASSERT_DOUBLE_EQ(*bbo.midPrice(), 102.0);
}

TEST_CASE(TestOrderBook_DepthQuery) {
    OrderBook book("BTC-USD");
    uint64_t next_tid = 1;

    book.addOrder(std::make_shared<Order>(1, "BTC-USD", Side::Buy, OrderType::Limit, 100.0, 10, 1), next_tid);
    book.addOrder(std::make_shared<Order>(2, "BTC-USD", Side::Buy, OrderType::Limit, 99.0, 20, 2), next_tid);
    book.addOrder(std::make_shared<Order>(3, "BTC-USD", Side::Buy, OrderType::Limit, 98.0, 30, 3), next_tid);
    book.addOrder(std::make_shared<Order>(4, "BTC-USD", Side::Buy, OrderType::Limit, 97.0, 40, 4), next_tid);

    auto depth = book.getDepth(Side::Buy, 3);
    ASSERT_EQ(depth.size(), 3);
    ASSERT_DOUBLE_EQ(depth[0].price, 100.0);
    ASSERT_EQ(depth[0].total_quantity, 10);
    ASSERT_DOUBLE_EQ(depth[1].price, 99.0);
    ASSERT_EQ(depth[1].total_quantity, 20);
    ASSERT_DOUBLE_EQ(depth[2].price, 98.0);
    ASSERT_EQ(depth[2].total_quantity, 30);
}
