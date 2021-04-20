// Copyright (c) 2020 Johannes Stoelp

#include "test_helper.h"

extern "C" {
#include <common.h>
#include <fmt.h>
}

void check_dec() {
    char have[16];
    int len = fmt(have, sizeof(have), "%d %d", 12345, -54321);

    ASSERT_EQ("12345 -54321", have);
    ASSERT_EQ(12, len);
    ASSERT_EQ('\0', have[len]);
}

void check_dec_long() {
    char have[32];
    int len = fmt(have, sizeof(have), "%ld %d", 8589934592 /* 2^33 */, 8589934592 /* 2^33 */);

    ASSERT_EQ("8589934592 0", have);
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

void check_hex_long() {
    char have[32];
    int len = fmt(have, sizeof(have), "%lx %x", 0x1111222233334444, 0x1111222233334444);

    ASSERT_EQ("1111222233334444 33334444", have);
    ASSERT_EQ(25, len);
    ASSERT_EQ('\0', have[len]);
}

void check_char() {
    char have[4];
    int len = fmt(have, sizeof(have), "%c%c%c", 'A', 'a', '\x01' /* non printable */);

    ASSERT_EQ("Aa\x01", have);
    ASSERT_EQ(3, len);
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

void check_memset() {
    unsigned char d[7] = {0};
    void* ret = memset(d, '\x42', sizeof(d));

    ASSERT_EQ(ret, d);
    for (unsigned i = 0; i < sizeof(d); ++i) {
        ASSERT_EQ(0x42, d[i]);
    }
}

void check_memcpy() {
    unsigned char s[5] = {5, 4, 3, 2, 1};
    unsigned char d[5] = {0};
    void* ret = memcpy(d, s, sizeof(d));

    ASSERT_EQ(ret, d);
    for (unsigned i = 0; i < sizeof(d); ++i) {
        ASSERT_EQ(5-i, d[i]);
    }
}

int main() {
    TEST_INIT;
    TEST_ADD(check_dec);
    TEST_ADD(check_dec_long);
    TEST_ADD(check_hex);
    TEST_ADD(check_hex_long);
    TEST_ADD(check_char);
    TEST_ADD(check_ptr);
    TEST_ADD(check_null);
    TEST_ADD(check_exact_len);
    TEST_ADD(check_exceed_len);
    TEST_ADD(check_memset);
    TEST_ADD(check_memcpy);
    return TEST_RUN;
}
