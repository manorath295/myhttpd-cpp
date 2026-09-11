// tests/test_main.cpp
// Test runner. All test cases register themselves at static init time via
// the TEST() macro in test_framework.hpp.

#include "test_framework.hpp"

int main() {
    int total = static_cast<int>(::test::cases().size());
    int passed = 0;
    for (auto& c : ::test::cases()) {
        int before = ::test::fails();
        std::fprintf(stderr, "[ RUN  ] %s\n", c.name.c_str());
        try { c.fn(); }
        catch (const std::exception& e) {
            std::fprintf(stderr, "  THROW %s\n", e.what());
            ::test::fails()++;
        }
        if (::test::fails() == before) {
            std::fprintf(stderr, "[  OK  ] %s\n", c.name.c_str());
            ++passed;
        } else {
            std::fprintf(stderr, "[ FAIL ] %s\n", c.name.c_str());
        }
    }
    std::fprintf(stderr, "\n=== %d/%d tests passed ===\n", passed, total);
    return ::test::fails() == 0 ? 0 : 1;
}
