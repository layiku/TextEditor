/*
 * test_encoding.c — encoding 模块单元测试
 * 覆盖：encoding_detect（纯内存），以及 Windows 上的 GBK↔UTF-8 转换
 */
#include <stdio.h>
#include <string.h>
#include "test_runner.h"
#include "../include/encoding.h"

/* ================================================================
 * encoding_detect 测试
 * ================================================================ */

static void test_detect_ascii(void) {
    TEST_CASE("encoding_detect: 纯 ASCII → ENC_UTF8");
    const char *s = "Hello, World!\n";
    ASSERT_EQ(encoding_detect(s, (int)strlen(s)), ENC_UTF8);
}

static void test_detect_empty(void) {
    TEST_CASE("encoding_detect: 空缓冲 → ENC_UTF8");
    ASSERT_EQ(encoding_detect("", 0), ENC_UTF8);
}

static void test_detect_utf8_bom(void) {
    TEST_CASE("encoding_detect: UTF-8 BOM 开头 → ENC_UTF8");
    const char bom[] = "\xEF\xBB\xBFhello";
    ASSERT_EQ(encoding_detect(bom, (int)sizeof(bom) - 1), ENC_UTF8);
}

static void test_detect_valid_utf8_multibyte(void) {
    TEST_CASE("encoding_detect: 合法多字节 UTF-8（中文）→ ENC_UTF8");
    /* UTF-8 编码的 "你好" = E4 BD A0 E5 A5 BD */
    const char s[] = "\xE4\xBD\xA0\xE5\xA5\xBD";
    ASSERT_EQ(encoding_detect(s, (int)sizeof(s) - 1), ENC_UTF8);
}

static void test_detect_invalid_utf8_is_gbk(void) {
    TEST_CASE("encoding_detect: 非法 UTF-8 序列 → ENC_GBK");
    /* GBK 编码的 "你好" = C4 E3 BA C3，在 UTF-8 中是非法序列 */
    const char s[] = "\xC4\xE3\xBA\xC3";
    ASSERT_EQ(encoding_detect(s, (int)sizeof(s) - 1), ENC_GBK);
}

static void test_detect_lone_continuation_is_gbk(void) {
    TEST_CASE("encoding_detect: 孤立续字节 → ENC_GBK");
    /* 0x80 单独出现不合法 */
    const char s[] = "\x80\x81";
    ASSERT_EQ(encoding_detect(s, (int)sizeof(s) - 1), ENC_GBK);
}

/* ================================================================
 * GBK ↔ UTF-8 转换测试（仅 Windows）
 * ================================================================ */

#ifdef _WIN32
static void test_gbk_to_utf8_ascii(void) {
    TEST_CASE("gbk_to_utf8_buf: ASCII 内容保持不变");
    const char *in = "Hello";
    char out[32] = {0};
    int n = gbk_to_utf8_buf(in, (int)strlen(in), out, (int)sizeof(out));
    ASSERT_EQ(n, 5);
    ASSERT_STR_EQ(out, "Hello");
}

static void test_utf8_to_gbk_ascii(void) {
    TEST_CASE("utf8_to_gbk_buf: ASCII 内容保持不变");
    const char *in = "World";
    char out[32] = {0};
    int n = utf8_to_gbk_buf(in, (int)strlen(in), out, (int)sizeof(out));
    ASSERT_EQ(n, 5);
    ASSERT_STR_EQ(out, "World");
}

static void test_gbk_utf8_roundtrip(void) {
    TEST_CASE("gbk_to_utf8_buf + utf8_to_gbk_buf: GBK→UTF-8→GBK 往返一致");
    /* GBK 编码的 "你好" = C4 E3 BA C3 */
    const char gbk_orig[] = "\xC4\xE3\xBA\xC3";
    int         orig_len  = 4;

    char utf8_buf[32] = {0};
    int  utf8_len = gbk_to_utf8_buf(gbk_orig, orig_len, utf8_buf, (int)sizeof(utf8_buf));
    ASSERT_TRUE(utf8_len > 0);

    /* 转回 GBK */
    char gbk_back[32] = {0};
    int  back_len = utf8_to_gbk_buf(utf8_buf, utf8_len, gbk_back, (int)sizeof(gbk_back));
    ASSERT_EQ(back_len, orig_len);
    ASSERT_EQ(memcmp(gbk_orig, gbk_back, (size_t)orig_len), 0);
}

static void test_gbk_to_utf8_small_buf(void) {
    TEST_CASE("gbk_to_utf8_buf: 输出缓冲太小返回 -1");
    /* GBK 的 "你" → UTF-8 需 3 字节，但只给 2 字节 */
    const char gbk[] = "\xC4\xE3";
    char out[2];
    int n = gbk_to_utf8_buf(gbk, 2, out, 2);
    ASSERT_EQ(n, -1);
}
#endif /* _WIN32 */

/* ================================================================
 * 入口
 * ================================================================ */

void test_encoding_run(void) {
    printf("\n[test_encoding]\n");

    test_detect_ascii();
    test_detect_empty();
    test_detect_utf8_bom();
    test_detect_valid_utf8_multibyte();
    test_detect_invalid_utf8_is_gbk();
    test_detect_lone_continuation_is_gbk();

#ifdef _WIN32
    test_gbk_to_utf8_ascii();
    test_utf8_to_gbk_ascii();
    test_gbk_utf8_roundtrip();
    test_gbk_to_utf8_small_buf();
#endif
}
