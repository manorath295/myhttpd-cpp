// tests/test_framework.hpp
// Tiny zero-dependency test framework header — included by every test_*.cpp
// and the test_main.cpp runner.

#pragma once

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace test {
struct Case { std::string name; std::function<void()> fn; };
inline std::vector<Case>& cases() { static std::vector<Case> v; return v; }
struct Register { Register(const char* n, std::function<void()> f) { cases().push_back({n, std::move(f)}); } };
inline int& fails() { static int x = 0; return x; }

// Generic stringify that works for ints, strings, and any ostream-able type.
template <typename T>
std::string str(const T& v) {
    std::ostringstream os; os << v; return os.str();
}
} // namespace test

#define TEST(name)                                                          \
    static void name();                                                     \
    static ::test::Register _reg_##name(#name, name);                      \
    static void name()

#define EXPECT_TRUE(cond) do {                                              \
    if (!(cond)) {                                                          \
        std::fprintf(stderr, "  FAIL %s:%d  EXPECT_TRUE(%s)\n",             \
                     __FILE__, __LINE__, #cond);                            \
        ::test::fails()++;                                                  \
    }                                                                       \
} while (0)

#define EXPECT_EQ(a, b) do {                                                \
    auto _av = (a); auto _bv = (b);                                         \
    if (!(_av == _bv)) {                                                    \
        std::fprintf(stderr,                                                 \
                     "  FAIL %s:%d  EXPECT_EQ(%s, %s)\n"                    \
                     "       got      = %s\n"                              \
                     "       expected = %s\n",                              \
                     __FILE__, __LINE__, #a, #b,                            \
                     ::test::str(_av).c_str(),                              \
                     ::test::str(_bv).c_str());                             \
        ::test::fails()++;                                                  \
    }                                                                       \
} while (0)
