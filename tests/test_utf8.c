/*
 * test_utf8.c — UTF-8 工具库单元测试
 * 覆盖：seq_len / decode / encode / cp_width / byte_to_col / col_to_byte /
 *       line_display_width / next_char / prev_char，边界与异常情况。
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "test_runner.h"
#include "../include/utf8.h"

/* ================================================================
 * utf8_seq_len
 * ================================================================ */

static void test_seq_len(void) {
    TEST_CASE("utf8_seq_len: ASCII/多字节/无效首字节");
    ASSERT_EQ(utf8_seq_len(0x00), 1);   /* NUL */
    ASSERT_EQ(utf8_seq_len(0x41), 1);   /* 'A' */
    ASSERT_EQ(utf8_seq_len(0x7F), 1);   /* DEL */
    ASSERT_EQ(utf8_seq_len(0xC2), 2);   /* 2 字节序列 */
    ASSERT_EQ(utf8_seq_len(0xE4), 3);   /* 3 字节序列（CJK） */
    ASSERT_EQ(utf8_seq_len(0xF0), 4);   /* 4 字节序列 */
    /* 续字节作为首字节 → 降级 1 */
    ASSERT_EQ(utf8_seq_len(0x80), 1);
    ASSERT_EQ(utf8_seq_len(0xBF), 1);
}

/* ================================================================
 * utf8_decode
 * ================================================================ */

static void test_decode_ascii(void) {
    TEST_CASE("utf8_decode: ASCII 字符");
    uint32_t cp = 0;
    int len = utf8_decode("A", &cp);
    ASSERT_EQ(len, 1);
    ASSERT_EQ((int)cp, 0x41);
}

static void test_decode_2byte(void) {
    TEST_CASE("utf8_decode: 2 字节序列 U+00E9 é");
    const char s[] = { (char)0xC3, (char)0xA9, '\0' };
    uint32_t cp = 0;
    int len = utf8_decode(s, &cp);
    ASSERT_EQ(len, 2);
    ASSERT_EQ((int)cp, 0xE9);
}

static void test_decode_3byte_cjk(void) {
    TEST_CASE("utf8_decode: 3 字节序列 U+4E2D 中");
    const char s[] = { (char)0xE4, (char)0xB8, (char)0xAD, '\0' };
    uint32_t cp = 0;
    int len = utf8_decode(s, &cp);
    ASSERT_EQ(len, 3);
    ASSERT_EQ((int)cp, 0x4E2D);
}

static void test_decode_invalid(void) {
    TEST_CASE("utf8_decode: 非法字节 → 替换字符 U+FFFD");
    const char s[] = { (char)0xFF, 'A', '\0' };
    uint32_t cp = 0;
    int len = utf8_decode(s, &cp);
    ASSERT_EQ(len, 1);
    ASSERT_EQ((int)cp, (int)0xFFFD);
}

/* ================================================================
 * utf8_encode + round-trip
 * ================================================================ */

static void test_encode_ascii(void) {
    TEST_CASE("utf8_encode: ASCII 'Z'");
    char buf[4] = {0};
    int len = utf8_encode('Z', buf);
    ASSERT_EQ(len, 1);
    ASSERT_EQ((int)(unsigned char)buf[0], 'Z');
}

static void test_encode_cjk(void) {
    TEST_CASE("utf8_encode: U+4E2D 中 → 3 字节");
    char buf[4] = {0};
    int len = utf8_encode(0x4E2D, buf);
    ASSERT_EQ(len, 3);
    ASSERT_EQ((int)(unsigned char)buf[0], 0xE4);
    ASSERT_EQ((int)(unsigned char)buf[1], 0xB8);
    ASSERT_EQ((int)(unsigned char)buf[2], 0xAD);
}

static void test_encode_roundtrip(void) {
    TEST_CASE("utf8_encode/decode: 往返测试");
    uint32_t cps[] = { 'A', 0xE9, 0x4E2D, 0x1F600 };
    int n = (int)(sizeof(cps) / sizeof(cps[0]));
    for (int i = 0; i < n; i++) {
        char buf[4];
        int elen = utf8_encode(cps[i], buf);
        uint32_t out = 0;
        int dlen = utf8_decode(buf, &out);
        ASSERT_EQ(elen, dlen);
        ASSERT_EQ((int)cps[i], (int)out);
    }
}

/* ================================================================
 * utf8_cp_width
 * ================================================================ */

static void test_cp_width(void) {
    TEST_CASE("utf8_cp_width: ASCII=1，CJK=2，Hangul=2，全角=2");
    ASSERT_EQ(utf8_cp_width(' '),    1);
    ASSERT_EQ(utf8_cp_width('A'),    1);
    ASSERT_EQ(utf8_cp_width(0x7E),  1);   /* '~' */
    ASSERT_EQ(utf8_cp_width(0x4E00), 2);  /* 一 */
    ASSERT_EQ(utf8_cp_width(0x4E2D), 2);  /* 中 */
    ASSERT_EQ(utf8_cp_width(0x9FFF), 2);  /* CJK 上限 */
    ASSERT_EQ(utf8_cp_width(0xAC00), 2);  /* 가 Hangul */
    ASSERT_EQ(utf8_cp_width(0xFF01), 2);  /* ！全角 */
}

/* ================================================================
 * utf8_byte_to_col
 * ================================================================ */

static void test_byte_to_col_ascii(void) {
    TEST_CASE("utf8_byte_to_col: 纯 ASCII");
    const char *s = "Hello";
    ASSERT_EQ(utf8_byte_to_col(s, 5, 0), 0);
    ASSERT_EQ(utf8_byte_to_col(s, 5, 3), 3);
    ASSERT_EQ(utf8_byte_to_col(s, 5, 5), 5);
}

static void test_byte_to_col_mixed(void) {
    TEST_CASE("utf8_byte_to_col: ASCII + CJK 混合");
    /* "AB中C"：A(1) B(1) 中(3bytes=2cols) C(1) → 字节总数 6 */
    const char s[] = { 'A', 'B',
                        (char)0xE4, (char)0xB8, (char)0xAD,
                        'C', '\0' };
    int len = 6;
    ASSERT_EQ(utf8_byte_to_col(s, len, 0), 0);  /* 'A' 前 */
    ASSERT_EQ(utf8_byte_to_col(s, len, 2), 2);  /* '中' 前，已有 2 ASCII */
    ASSERT_EQ(utf8_byte_to_col(s, len, 5), 4);  /* 'C' 前：2+2=4 列 */
    ASSERT_EQ(utf8_byte_to_col(s, len, 6), 5);  /* 行末 */
}

/* ================================================================
 * utf8_col_to_byte
 * ================================================================ */

static void test_col_to_byte_ascii(void) {
    TEST_CASE("utf8_col_to_byte: 纯 ASCII");
    const char *s = "Hello";
    ASSERT_EQ(utf8_col_to_byte(s, 5, 0), 0);
    ASSERT_EQ(utf8_col_to_byte(s, 5, 3), 3);
    ASSERT_EQ(utf8_col_to_byte(s, 5, 5), 5);
}

static void test_col_to_byte_mixed(void) {
    TEST_CASE("utf8_col_to_byte: 宽字符内部 col → 返回字符起始字节");
    /* "AB中C" */
    const char s[] = { 'A', 'B',
                        (char)0xE4, (char)0xB8, (char)0xAD,
                        'C', '\0' };
    int len = 6;
    ASSERT_EQ(utf8_col_to_byte(s, len, 2), 2);  /* '中' 起始 */
    ASSERT_EQ(utf8_col_to_byte(s, len, 3), 2);  /* 右半 col=3 → 返回起始 2 */
    ASSERT_EQ(utf8_col_to_byte(s, len, 4), 5);  /* 'C' 起始 */
    ASSERT_EQ(utf8_col_to_byte(s, len, 5), 6);  /* 行末 */
}

/* ================================================================
 * utf8_line_display_width
 * ================================================================ */

static void test_line_display_width(void) {
    TEST_CASE("utf8_line_display_width: 纯 ASCII 和 CJK");
    ASSERT_EQ(utf8_line_display_width("Hello", 5), 5);

    /* "中文" = 2 × 3 bytes → 4 display cols */
    const char cjk[] = { (char)0xE4, (char)0xB8, (char)0xAD,
                          (char)0xE6, (char)0x96, (char)0x87, '\0' };
    ASSERT_EQ(utf8_line_display_width(cjk, 6), 4);

    /* 空行 */
    ASSERT_EQ(utf8_line_display_width("", 0), 0);
}

/* ================================================================
 * utf8_next_char / utf8_prev_char
 * ================================================================ */

static void test_next_char_ascii(void) {
    TEST_CASE("utf8_next_char: ASCII 逐字节步进");
    const char *s = "ABC";
    ASSERT_EQ(utf8_next_char(s, 3, 0), 1);
    ASSERT_EQ(utf8_next_char(s, 3, 2), 3);
    ASSERT_EQ(utf8_next_char(s, 3, 3), 3);  /* 已在末尾 */
}

static void test_next_char_cjk(void) {
    TEST_CASE("utf8_next_char: CJK 按 3 字节步进");
    const char s[] = { (char)0xE4, (char)0xB8, (char)0xAD,
                        (char)0xE6, (char)0x96, (char)0x87, '\0' };
    ASSERT_EQ(utf8_next_char(s, 6, 0), 3);
    ASSERT_EQ(utf8_next_char(s, 6, 3), 6);
}

static void test_prev_char_ascii(void) {
    TEST_CASE("utf8_prev_char: ASCII 逐字节后退");
    const char *s = "ABC";
    ASSERT_EQ(utf8_prev_char(s, 3), 2);
    ASSERT_EQ(utf8_prev_char(s, 1), 0);
    ASSERT_EQ(utf8_prev_char(s, 0), 0);  /* 行首 */
}

static void test_prev_char_cjk(void) {
    TEST_CASE("utf8_prev_char: CJK 跳过续字节");
    const char s[] = { (char)0xE4, (char)0xB8, (char)0xAD,
                        (char)0xE6, (char)0x96, (char)0x87, '\0' };
    ASSERT_EQ(utf8_prev_char(s, 6), 3);  /* 退到'文'起始 */
    ASSERT_EQ(utf8_prev_char(s, 3), 0);  /* 退到'中'起始 */
}

/* ================================================================
 * 注册
 * ================================================================ */

void test_utf8_run(void) {
    test_seq_len();
    test_decode_ascii();
    test_decode_2byte();
    test_decode_3byte_cjk();
    test_decode_invalid();
    test_encode_ascii();
    test_encode_cjk();
    test_encode_roundtrip();
    test_cp_width();
    test_byte_to_col_ascii();
    test_byte_to_col_mixed();
    test_col_to_byte_ascii();
    test_col_to_byte_mixed();
    test_line_display_width();
    test_next_char_ascii();
    test_next_char_cjk();
    test_prev_char_ascii();
    test_prev_char_cjk();
}
