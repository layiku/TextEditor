/*
 * test_clipboard.c — clipboard 模块单元测试
 */
#include <stdio.h>
#include <string.h>
#include "test_runner.h"
#include "../include/clipboard.h"

static void test_set_get(void) {
    TEST_CASE("set/get: 基本存取");
    clipboard_init();
    clipboard_set("hello", 5);
    int len;
    const char *data = clipboard_get(&len);
    ASSERT_NOT_NULL(data);
    ASSERT_STR_EQ(data, "hello");
    ASSERT_EQ(len, 5);
    clipboard_exit();
}

static void test_multiline(void) {
    TEST_CASE("set/get: 多行内容（含 \\n）");
    clipboard_init();
    clipboard_set("line1\nline2", 11);
    int len;
    const char *data = clipboard_get(&len);
    ASSERT_STR_EQ(data, "line1\nline2");
    ASSERT_EQ(len, 11);
    clipboard_exit();
}

static void test_empty(void) {
    TEST_CASE("is_empty: 未设置时为空");
    clipboard_init();
    ASSERT_EQ(clipboard_is_empty(), 1);
    clipboard_exit();
}

static void test_overwrite(void) {
    TEST_CASE("set: 多次设置覆盖旧内容");
    clipboard_init();
    clipboard_set("first", 5);
    clipboard_set("second", 6);
    int len;
    const char *data = clipboard_get(&len);
    ASSERT_STR_EQ(data, "second");
    ASSERT_EQ(len, 6);
    clipboard_exit();
}

static void test_empty_string(void) {
    TEST_CASE("set: 设置空内容后 is_empty=1");
    clipboard_init();
    clipboard_set("abc", 3);
    clipboard_set(NULL, 0);
    ASSERT_EQ(clipboard_is_empty(), 1);
    clipboard_exit();
}

void test_clipboard_run(void) {
    printf("\n[模块] clipboard\n");
    test_set_get();
    test_multiline();
    test_empty();
    test_overwrite();
    test_empty_string();
}
