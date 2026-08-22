#include "test_framework.hpp"

int main() {
    return lobster::test::TestRegistry::instance().runAll();
}
