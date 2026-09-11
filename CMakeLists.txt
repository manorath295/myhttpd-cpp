cmake_minimum_required(VERSION 3.16)
project(myhttpd
    VERSION 2.0.0
    DESCRIPTION "A multithreaded HTTP web server written in modern C++17"
    LANGUAGES CXX)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# ---- Toolchain / global options -------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor)
endif()

option(MYHTTPD_BUILD_TESTS  "Build unit tests"               ON)
option(MYHTTPD_BUILD_BENCH  "Build benchmark tool"           ON)
option(MYHTTPD_USE_ASAN     "Enable AddressSanitizer in dev" OFF)

if(MYHTTPD_USE_ASAN)
    add_compile_options(-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address -fsanitize=undefined)
endif()

# ---- Core library (so tests and benchmarks can link against it) ----------
add_library(myhttpd_core STATIC
    src/http_parser.cpp
    src/http_response.cpp
    src/http_server.cpp
)
target_include_directories(myhttpd_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(myhttpd_core PUBLIC Threads::Threads)

# ---- Executable -----------------------------------------------------------
add_executable(myhttpd src/main.cpp)
target_link_libraries(myhttpd PRIVATE myhttpd_core)

# ---- Tests -----------------------------------------------------------------
if(MYHTTPD_BUILD_TESTS)
    enable_testing()
    add_executable(myhttpd_tests
        tests/test_main.cpp
        tests/test_socket_handle.cpp
        tests/test_http_parser.cpp
        tests/test_scheduler.cpp
        tests/test_logger.cpp
        tests/test_http_response.cpp
    )
    target_include_directories(myhttpd_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(myhttpd_tests PRIVATE myhttpd_core)
    target_compile_definitions(myhttpd_tests PRIVATE MYHTTPD_VERSION="${PROJECT_VERSION}")
    add_test(NAME myhttpd_unit_tests COMMAND myhttpd_tests)
endif()

# ---- Benchmark -------------------------------------------------------------
if(MYHTTPD_BUILD_BENCH)
    add_executable(myhttpd_bench benchmark/bench_main.cpp)
    target_link_libraries(myhttpd_bench PRIVATE myhttpd_core)
endif()

# ---- Install ---------------------------------------------------------------
install(TARGETS myhttpd RUNTIME DESTINATION bin)
