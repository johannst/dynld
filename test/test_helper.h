// Copyright (c) 2020 Johannes Stoelp

#pragma once
#include <cstring>
#include <exception>
#include <functional>
#include <vector>
#include <iostream>

// Extremely trivial helper, just to get some tests out.

struct TestFailed : std::exception {};

// Value based ASSERT_* helper.
//
// Requirements:
//   T1: comparable + printable (stream operator)
//   T2: comparable + printable (stream operator)

template<typename T1, typename T2>
void ASSERT_EQ(T1 expected, T2 have) {
    if (expected != have) {
        std::cerr << "ASSERT_EQ failed:\n"
                  << "  expected: " << expected << '\n'
                  << "  have    : " << have << '\n'
                  << std::flush;
        throw TestFailed{};
    }
}

// Char string based ASSERT_* helper.

template<>
void ASSERT_EQ(const char* expected, const char* have) {
    if (std::strcmp(expected, have) != 0) {
        std::cerr << "ASSERT_EQ failed:\n"
                  << "  expected: " << expected << '\n'
                  << "  have    : " << have << '\n'
                  << std::flush;
        throw TestFailed{};
    }
}

template<>
void ASSERT_EQ(const char* expected, char* have) {
    ASSERT_EQ(expected, static_cast<const char*>(have));
}

template<>
void ASSERT_EQ(char* expected, const char* have) {
    ASSERT_EQ(static_cast<const char*>(expected), have);
}

template<>
void ASSERT_EQ(char* expected, char* have) {
    ASSERT_EQ(static_cast<const char*>(expected), static_cast<const char*>(have));
}

// Simple test runner abstraction.

struct Runner {
    void addTest(const char* name, std::function<void()> fn) { mTests.push_back(Test{name, fn}); }

    int runTests() {
        unsigned fail_cnt = 0;
        for (auto test : mTests) {
            try {
                test.fn();
                std::cerr << "SUCCESS " << test.name << std::endl;
            } catch (TestFailed&) {
                ++fail_cnt;
                std::cerr << "FAIL    " << test.name << std::endl;
            } catch (...) {
                ++fail_cnt;
                std::cerr << "FAIL    " << test.name << "(caught unspecified exception)" << std::endl;
            }
        }
        return fail_cnt;
    }

  private:
    struct Test {
        const char* name;
        std::function<void()> fn;
    };

    std::vector<Test> mTests{};
};

#define TEST_INIT    Runner r;
#define TEST_ADD(fn) r.addTest(#fn, fn);
#define TEST_RUN     r.runTests();
