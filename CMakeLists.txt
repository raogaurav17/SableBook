cmake_minimum_required(VERSION 3.20)
project(SableBook
    VERSION 1.0.0
    DESCRIPTION "High-Performance Limit Order Book & Matching Engine in Modern C++"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── Sanitizer Support ────────────────────────────────────────────────────────
# Usage: cmake -B build -DSABLEBOOK_SANITIZE=asan_ubsan   (Address + UB)
#        cmake -B build -DSABLEBOOK_SANITIZE=tsan          (Thread)
#        cmake -B build                                     (none — default)
# ─────────────────────────────────────────────────────────────────────────────
set(SABLEBOOK_SANITIZE "" CACHE STRING
    "Enable sanitizers: 'asan_ubsan' for ASan+UBSan, 'tsan' for TSan, '' for none")
set_property(CACHE SABLEBOOK_SANITIZE PROPERTY STRINGS "" "asan_ubsan" "tsan")

set(SANITIZER_COMPILE_FLAGS "")
set(SANITIZER_LINK_FLAGS "")

if(SABLEBOOK_SANITIZE STREQUAL "asan_ubsan")
    message(STATUS "[SableBook] Sanitizers: AddressSanitizer + UndefinedBehaviorSanitizer")
    set(SANITIZER_COMPILE_FLAGS
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
        -fno-sanitize-recover=all
    )
    set(SANITIZER_LINK_FLAGS -fsanitize=address,undefined)
elseif(SABLEBOOK_SANITIZE STREQUAL "tsan")
    message(STATUS "[SableBook] Sanitizers: ThreadSanitizer")
    set(SANITIZER_COMPILE_FLAGS
        -fsanitize=thread
        -fno-omit-frame-pointer
    )
    set(SANITIZER_LINK_FLAGS -fsanitize=thread)
elseif(NOT SABLEBOOK_SANITIZE STREQUAL "")
    message(FATAL_ERROR "[SableBook] Unknown SABLEBOOK_SANITIZE value: '${SABLEBOOK_SANITIZE}'. Use 'asan_ubsan', 'tsan', or leave empty.")
endif()

# ─── Compiler warnings & optimizations ───────────────────────────────────────
if(MSVC)
    add_compile_options(/W4 /O2 /permissive-)
else()
    # Use -O1 under sanitizers (ASan/TSan conflict with -O3 + -march=native sometimes)
    if(SABLEBOOK_SANITIZE STREQUAL "")
        add_compile_options(-Wall -Wextra -Wpedantic -O3 -march=native)
    else()
        add_compile_options(-Wall -Wextra -Wpedantic -O1 -g)
    endif()
endif()

# Core Library
add_library(sablebook_core
    src/OrderBook.cpp
    src/MatchingEngine.cpp
)

target_compile_options(sablebook_core PRIVATE ${SANITIZER_COMPILE_FLAGS})
# PRIVATE: sanitizer link flags apply only to sablebook_core itself.
# sablebook_tests and sablebook_bench add their own SANITIZER_LINK_FLAGS below,
# so sablebook_cli (which has none) is not affected.
target_link_options(sablebook_core PRIVATE ${SANITIZER_LINK_FLAGS})

target_include_directories(sablebook_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# CLI Application — skipped under sanitizer builds.
# When sablebook_core is compiled with TSan/ASan, every .o in the .a carries
# sanitizer instrumentation calls (__tsan_read8, __asan_report_*, etc.).
# Any executable linking that .a must also supply the sanitizer runtime.
# sablebook_cli has no sanitizer link flags and is never run by ctest,
# so there is no value in building it during a sanitizer run.
if(SABLEBOOK_SANITIZE STREQUAL "")
    add_executable(sablebook_cli src/main.cpp)
    target_link_libraries(sablebook_cli PRIVATE sablebook_core)
endif()


# Tests
enable_testing()

add_executable(sablebook_tests
    tests/test_orderbook.cpp
    tests/test_matching.cpp
    tests/test_cancel.cpp
    tests/test_edge_cases.cpp
    tests/test_main.cpp
)
target_include_directories(sablebook_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
target_link_libraries(sablebook_tests PRIVATE sablebook_core)
target_compile_options(sablebook_tests PRIVATE ${SANITIZER_COMPILE_FLAGS})
target_link_options(sablebook_tests PRIVATE ${SANITIZER_LINK_FLAGS})

add_test(NAME AllTests COMMAND sablebook_tests)

# Benchmarks
add_executable(sablebook_bench
    benchmarks/latency_bench.cpp
)
target_include_directories(sablebook_bench
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(sablebook_bench PRIVATE sablebook_core)
target_compile_options(sablebook_bench PRIVATE ${SANITIZER_COMPILE_FLAGS})
target_link_options(sablebook_bench PRIVATE ${SANITIZER_LINK_FLAGS})
