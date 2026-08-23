#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <sstream>
#include <cmath>

namespace sablebook::test {

struct TestCase {
    std::string name;
    std::function<void()> func;
};

class TestRegistry {
public:
    static TestRegistry& instance() {
        static TestRegistry reg;
        return reg;
    }

    void addTest(const std::string& name, std::function<void()> func) {
        tests_.push_back({name, std::move(func)});
    }

    int runAll() {
        int passed = 0;
        int failed = 0;
        std::cout << "\n======================================================\n";
        std::cout << "           Running SableBook Test Suite\n";
        std::cout << "======================================================\n";

        for (const auto& tc : tests_) {
            try {
                tc.func();
                std::cout << "  [PASS] " << tc.name << "\n";
                passed++;
            } catch (const std::exception& e) {
                std::cout << "  [FAIL] " << tc.name << "\n";
                std::cout << "         Error: " << e.what() << "\n";
                failed++;
            } catch (...) {
                std::cout << "  [FAIL] " << tc.name << " (Unknown exception)\n";
                failed++;
            }
        }

        std::cout << "======================================================\n";
        std::cout << "  Summary: " << passed << " passed, " << failed << " failed, "
                  << (passed + failed) << " total.\n";
        std::cout << "======================================================\n\n";

        return failed == 0 ? 0 : 1;
    }

private:
    std::vector<TestCase> tests_;
};

struct AutoRegister {
    AutoRegister(const std::string& name, std::function<void()> func) {
        TestRegistry::instance().addTest(name, std::move(func));
    }
};

#define TEST_CASE(name) \
    void name(); \
    static ::sablebook::test::AutoRegister auto_reg_##name(#name, name); \
    void name()

inline void assertTrue(bool cond, const std::string& msg, const char* file, int line) {
    if (!cond) {
        std::ostringstream ss;
        ss << file << ":" << line << " Assertion failed: " << msg;
        throw std::runtime_error(ss.str());
    }
}

inline void assertEqualDouble(double a, double b, double eps, const std::string& msg, const char* file, int line) {
    if (std::abs(a - b) > eps) {
        std::ostringstream ss;
        ss << file << ":" << line << " Double assertion failed (" << a << " != " << b << "): " << msg;
        throw std::runtime_error(ss.str());
    }
}

#define ASSERT_TRUE(cond) ::sablebook::test::assertTrue((cond), #cond, __FILE__, __LINE__)
#define ASSERT_FALSE(cond) ::sablebook::test::assertTrue(!(cond), "!(" #cond ")", __FILE__, __LINE__)
#define ASSERT_EQ(a, b) ::sablebook::test::assertTrue((a) == (b), #a " == " #b, __FILE__, __LINE__)
#define ASSERT_NE(a, b) ::sablebook::test::assertTrue((a) != (b), #a " != " #b, __FILE__, __LINE__)
#define ASSERT_DOUBLE_EQ(a, b) ::sablebook::test::assertEqualDouble((a), (b), 1e-6, #a " == " #b, __FILE__, __LINE__)

} // namespace sablebook::test
