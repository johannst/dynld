// Copyright (c) 2020 Johannes Stoelp

#include "test_helper.h"

#include <cstdio>
#include <fmt.h>

void check_dec() {
    char have[16];
    int len = fmt(have, sizeof(have), "%d %d", 12345, -54321);

    ASSERT_EQ("12345 -54321", have);
    ASSERT_EQ(12, len);
    ASSERT_EQ('\0', have[len]);
}

void check_hex() {
    char have[16];
    int len = fmt(have, sizeof(have), "%x %x", 0xdeadbeef, 0xcafe);

    ASSERT_EQ("deadbeef cafe", have);
    ASSERT_EQ(13, len);
    ASSERT_EQ('\0', have[len]);
}

void check_ptr() {
    char have[16];
    int len = fmt(have, sizeof(have), "%p %p", (void*)0xabcd, (void*)0x0);

    ASSERT_EQ("0xabcd 0x0", have);
    ASSERT_EQ(10, len);
    ASSERT_EQ('\0', have[len]);
}

void check_null() {
    int len = fmt(0, 0, "%s", "abcd1234efgh5678");

    ASSERT_EQ(16, len);
}

void check_exact_len() {
    char have[8];
    int len = fmt(have, sizeof(have), "%s", "12345678");

    ASSERT_EQ("1234567", have);
    ASSERT_EQ(8, len);
    ASSERT_EQ('\0', have[7]);
}

void check_exceed_len() {
    char have[8];
    int len = fmt(have, sizeof(have), "%s", "123456789abcedf");

    ASSERT_EQ("1234567", have);
    ASSERT_EQ(15, len);
    ASSERT_EQ('\0', have[7]);
}

int main() {
    TEST_INIT;
    TEST_ADD(check_dec);
    TEST_ADD(check_hex);
    TEST_ADD(check_ptr);
    TEST_ADD(check_null);
    TEST_ADD(check_exact_len);
    TEST_ADD(check_exceed_len);
    return TEST_RUN;
}
