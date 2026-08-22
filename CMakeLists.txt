cmake_minimum_required(VERSION 3.20)
project(LOBster
    VERSION 1.0.0
    DESCRIPTION "High-Performance Limit Order Book & Matching Engine in Modern C++"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization and compiler warnings
if(MSVC)
    add_compile_options(/W4 /O2 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -O3 -march=native)
endif()

# Core Library
add_library(lobster_core
    src/OrderBook.cpp
    src/MatchingEngine.cpp
)

target_include_directories(lobster_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# CLI Application
add_executable(lobster_cli
    src/main.cpp
)
target_link_libraries(lobster_cli PRIVATE lobster_core)

# Tests
enable_testing()

add_executable(lobster_tests
    tests/test_orderbook.cpp
    tests/test_matching.cpp
    tests/test_cancel.cpp
    tests/test_edge_cases.cpp
    tests/test_main.cpp
)
target_include_directories(lobster_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
target_link_libraries(lobster_tests PRIVATE lobster_core)

add_test(NAME AllTests COMMAND lobster_tests)

# Benchmarks
add_executable(lobster_bench
    benchmarks/latency_bench.cpp
)
target_include_directories(lobster_bench
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(lobster_bench PRIVATE lobster_core)
