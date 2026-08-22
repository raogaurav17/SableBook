#include "MatchingEngine.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <thread>
#include <random>

using namespace lobster;

// ANSI Color Codes
namespace Color {
    const char* Reset   = "\033[0m";
    const char* Bold    = "\033[1m";
    const char* Red     = "\033[31m";
    const char* Green   = "\033[32m";
    const char* Yellow  = "\033[33m";
    const char* Blue    = "\033[34m";
    const char* Magenta = "\033[35m";
    const char* Cyan    = "\033[36m";
    const char* Gray    = "\033[90m";
}

void printHeader() {
    std::cout << Color::Cyan << Color::Bold;
    std::cout << R"(
  _      ____  ____       _            
 | |    / __ \|  _ \     | |           
 | |   | |  | | |_) | ___| |_ ___ _ __ 
 | |   | |  | |  _ < / __| __/ _ \ '__|
 | |___| |__| | |_) |\__ \ ||  __/ |   
 |______\____/|____/ |___/\__\___|_|   
    Limit Order Book & Matching Engine
)" << Color::Reset << "\n";
}

void displayOrderBook(const MatchingEngine& engine, const std::string& symbol = "DEFAULT", size_t levels = 5) {
    auto asks = engine.getDepth(Side::Sell, levels, symbol);
    auto bids = engine.getDepth(Side::Buy, levels, symbol);
    BBO bbo = engine.getBBO(symbol);

    std::cout << "\n" << Color::Bold << "=== ORDER BOOK [" << symbol << "] ===" << Color::Reset << "\n";
    std::cout << Color::Gray << "--------------------------------------------------------" << Color::Reset << "\n";
    std::cout << std::left << std::setw(12) << "Side"
              << std::setw(14) << "Price ($)"
              << std::setw(14) << "Quantity"
              << std::setw(10) << "Orders" << "\n";
    std::cout << Color::Gray << "--------------------------------------------------------" << Color::Reset << "\n";

    // Asks displayed in reverse (highest price at top down to lowest ask at bottom)
    for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
        std::cout << Color::Red
                  << std::left << std::setw(12) << "ASK"
                  << std::setw(14) << std::fixed << std::setprecision(2) << it->price
                  << std::setw(14) << it->total_quantity
                  << std::setw(10) << it->order_count
                  << Color::Reset << "\n";
    }

    if (asks.empty()) {
        std::cout << Color::Gray << "  [No resting asks]" << Color::Reset << "\n";
    }

    // BBO / Spread line
    std::cout << Color::Yellow << Color::Bold;
    std::cout << "--- SPREAD: ";
    if (bbo.spread().has_value()) {
        std::cout << "$" << std::fixed << std::setprecision(2) << *bbo.spread()
                  << " | MID: $" << *bbo.midPrice();
    } else {
        std::cout << "N/A";
    }
    std::cout << " ---" << Color::Reset << "\n";

    // Bids displayed highest price first
    for (const auto& level : bids) {
        std::cout << Color::Green
                  << std::left << std::setw(12) << "BID"
                  << std::setw(14) << std::fixed << std::setprecision(2) << level.price
                  << std::setw(14) << level.total_quantity
                  << std::setw(10) << level.order_count
                  << Color::Reset << "\n";
    }

    if (bids.empty()) {
        std::cout << Color::Gray << "  [No resting bids]" << Color::Reset << "\n";
    }

    std::cout << Color::Gray << "--------------------------------------------------------" << Color::Reset << "\n\n";
}

void runLiveSimulation(MatchingEngine& engine, size_t count = 30) {
    std::cout << Color::Yellow << "Starting live order simulation (" << count << " events)...\n" << Color::Reset;

    std::mt19937_64 rng(1337);
    std::uniform_real_distribution<double> bid_p(98.0, 100.0);
    std::uniform_real_distribution<double> ask_p(100.0, 102.0);
    std::uniform_int_distribution<uint64_t> qty_dist(5, 30);
    std::uniform_int_distribution<int> type_dist(1, 100);

    for (size_t i = 1; i <= count; ++i) {
        int r = type_dist(rng);
        if (r <= 45) {
            double p = std::round(bid_p(rng) * 100.0) / 100.0;
            engine.submitLimitOrder(Side::Buy, p, qty_dist(rng), "BTC-USD");
        } else if (r <= 90) {
            double p = std::round(ask_p(rng) * 100.0) / 100.0;
            engine.submitLimitOrder(Side::Sell, p, qty_dist(rng), "BTC-USD");
        } else {
            Side s = (rng() % 2 == 0) ? Side::Buy : Side::Sell;
            engine.submitMarketOrder(s, qty_dist(rng), "BTC-USD");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }

    displayOrderBook(engine, "BTC-USD", 6);
}

void printHelp() {
    std::cout << Color::Bold << "Available Commands:\n" << Color::Reset
              << "  " << Color::Green << "limit buy <price> <qty> [symbol]" << Color::Reset << "    : Submit Limit Buy Order\n"
              << "  " << Color::Red   << "limit sell <price> <qty> [symbol]" << Color::Reset << "   : Submit Limit Sell Order\n"
              << "  " << Color::Green << "market buy <qty> [symbol]" << Color::Reset << "           : Submit Market Buy Order\n"
              << "  " << Color::Red   << "market sell <qty> [symbol]" << Color::Reset << "          : Submit Market Sell Order\n"
              << "  " << Color::Yellow<< "cancel <order_id>" << Color::Reset << "                   : Cancel Order\n"
              << "  " << Color::Yellow<< "modify <order_id> <price> <qty>" << Color::Reset << "     : Modify Order Price/Qty\n"
              << "  " << Color::Cyan  << "status <order_id>" << Color::Reset << "                   : Query Order Status\n"
              << "  " << Color::Cyan  << "book [symbol] [levels]" << Color::Reset << "              : View Order Book Depth\n"
              << "  " << Color::Cyan  << "bbo [symbol]" << Color::Reset << "                        : Query Best Bid / Best Offer\n"
              << "  " << Color::Magenta<< "sim [count]" << Color::Reset << "                        : Run live market simulation\n"
              << "  " << Color::Magenta<< "stats" << Color::Reset << "                              : Display engine statistics & latency\n"
              << "  " << Color::Gray  << "help" << Color::Reset << "                               : Show this help message\n"
              << "  " << Color::Gray  << "exit / quit" << Color::Reset << "                        : Exit LOBster CLI\n\n";
}

int main() {
    MatchingEngine engine;
    engine.registerSymbol("BTC-USD");
    engine.registerSymbol("ETH-USD");

    engine.setTradeCallback([](const Trade& trade) {
        std::cout << Color::Magenta << Color::Bold
                  << "  [TRADE #" << trade.trade_id << "] "
                  << trade.symbol << " "
                  << trade.quantity << " @ $"
                  << std::fixed << std::setprecision(2) << trade.price
                  << " (Buy: #" << trade.buy_order_id
                  << ", Sell: #" << trade.sell_order_id << ")"
                  << Color::Reset << "\n";
    });

    engine.setRejectCallback([](uint64_t order_id, RejectReason reason) {
        std::cout << Color::Red << "  [REJECT] Order #" << order_id
                  << " rejected: " << toString(reason)
                  << Color::Reset << "\n";
    });

    printHeader();
    std::cout << "Type " << Color::Bold << "'help'" << Color::Reset << " for available commands or "
              << Color::Bold << "'sim'" << Color::Reset << " to run a demo simulation.\n\n";

    std::string line;
    while (true) {
        std::cout << Color::Bold << "LOBster> " << Color::Reset;
        if (!std::getline(std::cin, line)) break;

        std::istringstream iss(line);
        std::string cmd;
        if (!(iss >> cmd)) continue;

        if (cmd == "exit" || cmd == "quit") {
            std::cout << "Exiting LOBster. Goodbye!\n";
            break;
        } else if (cmd == "help") {
            printHelp();
        } else if (cmd == "sim") {
            size_t count = 25;
            iss >> count;
            runLiveSimulation(engine, count);
        } else if (cmd == "stats") {
            std::cout << "Total Orders Processed: " << engine.totalOrdersProcessed() << "\n";
            std::cout << "Total Trades Generated: " << engine.totalTradesGenerated() << "\n";
            engine.latencyTracker().printReport("Matching Engine Latency");
        } else if (cmd == "book") {
            std::string symbol = "DEFAULT";
            size_t levels = 5;
            iss >> symbol;
            if (iss >> levels) {}
            displayOrderBook(engine, symbol, levels);
        } else if (cmd == "bbo") {
            std::string symbol = "DEFAULT";
            iss >> symbol;
            BBO bbo = engine.getBBO(symbol);
            if (bbo.hasBid()) {
                std::cout << "Best Bid: $" << bbo.best_bid->price << " (Qty: " << bbo.best_bid->total_quantity << ")\n";
            } else {
                std::cout << "Best Bid: None\n";
            }
            if (bbo.hasAsk()) {
                std::cout << "Best Ask: $" << bbo.best_ask->price << " (Qty: " << bbo.best_ask->total_quantity << ")\n";
            } else {
                std::cout << "Best Ask: None\n";
            }
            if (bbo.spread().has_value()) {
                std::cout << "Spread:   $" << *bbo.spread() << "\n";
            }
        } else if (cmd == "limit") {
            std::string side_str;
            double price = 0.0;
            uint64_t qty = 0;
            std::string symbol = "DEFAULT";

            if (iss >> side_str >> price >> qty) {
                if (!(iss >> symbol)) symbol = "DEFAULT";
                Side side = (side_str == "buy" || side_str == "Buy") ? Side::Buy : Side::Sell;
                RejectReason reason;
                uint64_t id = engine.submitOrder(symbol, side, OrderType::Limit, price, qty, &reason);
                if (id > 0) {
                    std::cout << "Submitted Limit " << toString(side) << " Order #" << id
                              << " (" << qty << " @ $" << price << ")\n";
                }
            } else {
                std::cout << "Usage: limit <buy|sell> <price> <qty> [symbol]\n";
            }
        } else if (cmd == "market") {
            std::string side_str;
            uint64_t qty = 0;
            std::string symbol = "DEFAULT";

            if (iss >> side_str >> qty) {
                if (!(iss >> symbol)) symbol = "DEFAULT";
                Side side = (side_str == "buy" || side_str == "Buy") ? Side::Buy : Side::Sell;
                RejectReason reason;
                uint64_t id = engine.submitOrder(symbol, side, OrderType::Market, 0.0, qty, &reason);
                if (id > 0) {
                    std::cout << "Submitted Market " << toString(side) << " Order #" << id
                              << " (" << qty << " units)\n";
                }
            } else {
                std::cout << "Usage: market <buy|sell> <qty> [symbol]\n";
            }
        } else if (cmd == "cancel") {
            uint64_t id = 0;
            if (iss >> id) {
                bool ok = engine.cancelOrder(id);
                if (ok) {
                    std::cout << "Order #" << id << " successfully cancelled.\n";
                } else {
                    std::cout << "Failed to cancel Order #" << id << " (not found or terminal).\n";
                }
            } else {
                std::cout << "Usage: cancel <order_id>\n";
            }
        } else if (cmd == "modify") {
            uint64_t id = 0;
            double price = 0.0;
            uint64_t qty = 0;
            if (iss >> id >> price >> qty) {
                bool ok = engine.modifyOrder(id, price, qty);
                if (ok) {
                    std::cout << "Order #" << id << " modified to " << qty << " @ $" << price << "\n";
                } else {
                    std::cout << "Failed to modify Order #" << id << ".\n";
                }
            } else {
                std::cout << "Usage: modify <order_id> <price> <qty>\n";
            }
        } else if (cmd == "status") {
            uint64_t id = 0;
            if (iss >> id) {
                auto st = engine.getOrderStatus(id);
                if (st.has_value()) {
                    std::cout << "Order #" << id << " Status: " << toString(*st) << "\n";
                    OrderPtr ord = engine.getOrder(id);
                    if (ord) {
                        std::cout << "  Symbol:    " << ord->symbol << "\n"
                                  << "  Side:      " << toString(ord->side) << "\n"
                                  << "  Type:      " << toString(ord->type) << "\n"
                                  << "  Price:     $" << ord->price << "\n"
                                  << "  Orig Qty:  " << ord->quantity << "\n"
                                  << "  Rem Qty:   " << ord->remaining_quantity << "\n"
                                  << "  Filled:    " << ord->filledQuantity() << "\n";
                    }
                } else {
                    std::cout << "Order #" << id << " not found.\n";
                }
            } else {
                std::cout << "Usage: status <order_id>\n";
            }
        } else {
            std::cout << "Unknown command: '" << cmd << "'. Type 'help' for available commands.\n";
        }
    }

    return 0;
}
