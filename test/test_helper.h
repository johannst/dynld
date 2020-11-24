// Copyright (c) 2020 Johannes Stoelp

#pragma once
#include <cstring>
#include <exception>
#include <iostream>

/* Extremely trivial helper, just to get some tests out. */

struct TestFailed : std::exception {};

/* Requirements
 * T1: comparable + printable (stream operator)
 * T2: comparable + printable (stream operator)
 */

template<typename T1, typename T2>
void ASSERT_EQ(T1 expected, T2 have) {
    if (expected != have) {
        std::cerr << "ASSERT_EQ failed:\n"
                  << "  expected: " << expected << "\n"
                  << "  have    : " << have << "\n"
                  << std::flush;
        throw TestFailed{};
    }
}

template<typename T1, typename T2>
void ASSERT_EQ(T1* expected, T2* have) {
    ASSERT_EQ(*expected, *have);
}

template<>
void ASSERT_EQ(const char* expected, const char* have) {
    if (std::strcmp(expected, have) != 0) {
        std::cerr << "ASSERT_EQ failed:\n"
                  << "  expected: " << expected << "\n"
                  << "  have    : " << have << "\n"
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

void ASSERT_EQ(char* expected, char* have) {
    ASSERT_EQ(static_cast<const char*>(expected), static_cast<const char*>(have));
}

#define TEST_INIT     unsigned fail_cnt = 0;
#define TEST_FAIL_CNT fail_cnt

#define TEST(fn)                                                                          \
    {                                                                                     \
        try {                                                                             \
            fn();                                                                         \
            std::cerr << "SUCCESS " #fn << std::endl;                                     \
        } catch (TestFailed&) {                                                           \
            ++fail_cnt;                                                                   \
            std::cerr << "FAIL    " #fn << std::endl;                                     \
        } catch (...) {                                                                   \
            ++fail_cnt;                                                                   \
            std::cerr << "FAIL    " #fn << "(caught unspecified exception)" << std::endl; \
        }                                                                                 \
    }
