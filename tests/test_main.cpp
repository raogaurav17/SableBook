#include "test_framework.hpp"

int main() {
    return sablebook::test::TestRegistry::instance().runAll();
}
