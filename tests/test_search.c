/*
 * test_search.c — search 模块单元测试
 */
#include <stdio.h>
#include <string.h>
#include "test_runner.h"
#include "../include/search.h"
#include "../include/document.h"

static Document* make_doc(const char *text) {
    Document *doc = document_new();
    int out_row, out_col;
    int len = (int)strlen(text);
    if (len > 0)
        document_insert_text(doc, 0, 0, text, len, &out_row, &out_col);
    return doc;
}

static void test_find_basic(void) {
    TEST_CASE("search_next: 基本查找");
    Document *doc = make_doc("foo bar foo");
    search_init("foo", "", SEARCH_CASE_SENSITIVE);
    int row, col;
    int match_len = search_next(doc, 0, 0, &row, &col);
    ASSERT_EQ(match_len, 3);
    ASSERT_EQ(row, 0);
    ASSERT_EQ(col, 0);
    document_free(doc);
}

static void test_find_next(void) {
    TEST_CASE("search_next: 连续查找找第二处");
    Document *doc = make_doc("foo bar foo");
    search_init("foo", "", SEARCH_CASE_SENSITIVE);
    int row, col;
    search_next(doc, 0, 0, &row, &col);           /* 第一处：(0,0) */
    int len = search_next(doc, row, col + 1, &row, &col); /* 第二处 */
    ASSERT_EQ(len, 3);
    ASSERT_EQ(col, 8);
    document_free(doc);
}

static void test_find_not_found(void) {
    TEST_CASE("search_next: 未找到返回 -1");
    Document *doc = make_doc("hello world");
    search_init("zzz", "", SEARCH_CASE_SENSITIVE);
    int row, col;
    ASSERT_EQ(search_next(doc, 0, 0, &row, &col), -1);
    document_free(doc);
}

static void test_find_case_insensitive(void) {
    TEST_CASE("search_next: 大小写不敏感");
    Document *doc = make_doc("Hello World");
    search_init("hello", "", 0);  /* 不区分大小写 */
    int row, col;
    int len = search_next(doc, 0, 0, &row, &col);
    ASSERT_EQ(len, 5);
    ASSERT_EQ(col, 0);
    document_free(doc);
}

static void test_find_prev(void) {
    TEST_CASE("search_prev: 反向查找");
    Document *doc = make_doc("foo bar foo");
    search_init("foo", "", SEARCH_CASE_SENSITIVE);
    int row, col;
    /* 从位置 (0, 11) 反向找，应找到第二个 foo (0,8) */
    int len = search_prev(doc, 0, 11, &row, &col);
    ASSERT_EQ(len, 3);
    ASSERT_EQ(col, 8);
    document_free(doc);
}

static void test_replace_current(void) {
    TEST_CASE("replace_current: 替换当前匹配");
    Document *doc = make_doc("foo bar foo");
    search_init("foo", "X", SEARCH_CASE_SENSITIVE);
    /* 替换 (0,0) 长度 3 的 "foo" */
    ASSERT_EQ(search_replace_current(doc, 0, 0, 3), 0);
    ASSERT_STR_EQ(document_get_line(doc, 0), "X bar foo");
    document_free(doc);
}

static void test_replace_all(void) {
    TEST_CASE("replace_all: 全部替换");
    Document *doc = make_doc("foo bar foo");
    search_init("foo", "X", SEARCH_CASE_SENSITIVE);
    int count = search_replace_all(doc);
    ASSERT_EQ(count, 2);
    ASSERT_STR_EQ(document_get_line(doc, 0), "X bar X");
    document_free(doc);
}

static void test_find_multiline(void) {
    TEST_CASE("search_next: 跨行查找");
    Document *doc = make_doc("abc\nfoo\nbar");
    search_init("foo", "", SEARCH_CASE_SENSITIVE);
    int row, col;
    int len = search_next(doc, 0, 0, &row, &col);
    ASSERT_EQ(len, 3);
    ASSERT_EQ(row, 1);
    ASSERT_EQ(col, 0);
    document_free(doc);
}

void test_search_run(void) {
    printf("\n[模块] search\n");
    test_find_basic();
    test_find_next();
    test_find_not_found();
    test_find_case_insensitive();
    test_find_prev();
    test_replace_current();
    test_replace_all();
    test_find_multiline();
}
