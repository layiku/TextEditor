/*
 * test_document.c — document 模块单元测试
 */
#include <stdio.h>
#include <string.h>
#include "test_runner.h"
#include "../include/document.h"

/* ---- 工具：快速断言行内容 ---- */
#define ASSERT_LINE(doc, row, expected) \
    ASSERT_STR_EQ(document_get_line((doc), (row)), (expected))

/* ================================================================
 * 测试用例
 * ================================================================ */

static void test_new(void) {
    TEST_CASE("document_new: 新文档有 1 行空行");
    Document *doc = document_new();
    ASSERT_NOT_NULL(doc);
    ASSERT_EQ(document_line_count(doc), 1);
    ASSERT_LINE(doc, 0, "");
    ASSERT_EQ(doc->modified, 0);
    document_free(doc);
}

static void test_insert_char(void) {
    TEST_CASE("insert_char: 在空行插入字符");
    Document *doc = document_new();
    document_insert_char(doc, 0, 0, 'H', false);
    document_insert_char(doc, 0, 1, 'i', false);
    ASSERT_LINE(doc, 0, "Hi");
    ASSERT_EQ(document_get_line_len(doc, 0), 2);
    ASSERT_EQ(doc->modified, 1);
    document_free(doc);
}

static void test_insert_middle(void) {
    TEST_CASE("insert_char: 在中间位置插入");
    Document *doc = document_new();
    document_insert_char(doc, 0, 0, 'a', false);
    document_insert_char(doc, 0, 1, 'c', false);
    document_insert_char(doc, 0, 1, 'b', false);  /* 在 a 后插入 b */
    ASSERT_LINE(doc, 0, "abc");
    document_free(doc);
}

static void test_delete_char(void) {
    TEST_CASE("delete_char: 删除中间字符");
    Document *doc = document_new();
    document_insert_char(doc, 0, 0, 'a', false);
    document_insert_char(doc, 0, 1, 'x', false);
    document_insert_char(doc, 0, 2, 'b', false);
    document_delete_char(doc, 0, 1);  /* 删除 'x' */
    ASSERT_LINE(doc, 0, "ab");
    document_free(doc);
}

static void test_backspace(void) {
    TEST_CASE("backspace: 删除前一字符");
    Document *doc = document_new();
    document_insert_char(doc, 0, 0, 'a', false);
    document_insert_char(doc, 0, 1, 'b', false);
    document_insert_char(doc, 0, 2, 'c', false);
    document_backspace(doc, 0, 2);  /* 删除 col=1 的 'b' */
    ASSERT_LINE(doc, 0, "ac");
    document_free(doc);
}

static void test_break_line(void) {
    TEST_CASE("break_line: 在行中间换行");
    Document *doc = document_new();
    document_insert_char(doc, 0, 0, 'a', false);
    document_insert_char(doc, 0, 1, 'b', false);
    document_insert_char(doc, 0, 2, 'c', false);
    document_break_line(doc, 0, 1);  /* 在 'a' 和 'b' 之间换行 */
    ASSERT_EQ(document_line_count(doc), 2);
    ASSERT_LINE(doc, 0, "a");
    ASSERT_LINE(doc, 1, "bc");
    document_free(doc);
}

static void test_merge_line(void) {
    TEST_CASE("merge_line (backspace 行首): 合并两行");
    Document *doc = document_new();
    document_insert_char(doc, 0, 0, 'a', false);
    document_break_line(doc, 0, 1);
    document_insert_char(doc, 1, 0, 'b', false);
    /* 在行 1 col=0 处 Backspace → 合并行 */
    document_backspace(doc, 1, 0);
    ASSERT_EQ(document_line_count(doc), 1);
    ASSERT_LINE(doc, 0, "ab");
    document_free(doc);
}

static void test_delete_at_line_end(void) {
    TEST_CASE("delete_char 行尾: 合并下一行");
    Document *doc = document_new();
    document_insert_char(doc, 0, 0, 'a', false);
    document_break_line(doc, 0, 1);
    document_insert_char(doc, 1, 0, 'b', false);
    /* 在 row=0 col=1（行尾）Delete → 合并行 1 */
    document_delete_char(doc, 0, 1);
    ASSERT_EQ(document_line_count(doc), 1);
    ASSERT_LINE(doc, 0, "ab");
    document_free(doc);
}

static void test_insert_text(void) {
    TEST_CASE("insert_text: 插入多字符（含换行）");
    Document *doc = document_new();
    int out_row, out_col;
    document_insert_text(doc, 0, 0, "hello\nworld", 11, &out_row, &out_col);
    ASSERT_EQ(document_line_count(doc), 2);
    ASSERT_LINE(doc, 0, "hello");
    ASSERT_LINE(doc, 1, "world");
    ASSERT_EQ(out_row, 1);
    ASSERT_EQ(out_col, 5);
    document_free(doc);
}

static void test_delete_range(void) {
    TEST_CASE("delete_range: 删除跨行范围");
    Document *doc = document_new();
    int or_, oc;
    document_insert_text(doc, 0, 0, "abc\nxyz", 7, &or_, &oc);
    /* 删除 (0,1) 到 (1,2)：即 "bc\nxy" */
    char buf[32];
    int deleted = document_delete_range(doc, 0, 1, 1, 2, buf, sizeof(buf));
    ASSERT_EQ(document_line_count(doc), 1);
    ASSERT_LINE(doc, 0, "az");
    ASSERT_EQ(deleted, 5);  /* "bc\nxy" = 5 字符 */
    (void)or_; (void)oc;
    document_free(doc);
}

static void test_undo_insert(void) {
    TEST_CASE("undo: 撤销插入字符");
    Document *doc = document_new();
    document_insert_char(doc, 0, 0, 'x', false);
    ASSERT_LINE(doc, 0, "x");
    int row = 0, col = 0;
    ASSERT_EQ(document_undo(doc, &row, &col), 0);
    ASSERT_LINE(doc, 0, "");
    ASSERT_EQ(row, 0);
    document_free(doc);
}

static void test_undo_delete(void) {
    TEST_CASE("undo: 撤销删除字符");
    Document *doc = document_new();
    document_insert_char(doc, 0, 0, 'a', false);
    document_insert_char(doc, 0, 1, 'b', false);
    document_delete_char(doc, 0, 0);  /* 删除 'a' */
    ASSERT_LINE(doc, 0, "b");
    int row, col;
    document_undo(doc, &row, &col);
    ASSERT_LINE(doc, 0, "ab");
    document_free(doc);
}

static void test_redo(void) {
    TEST_CASE("redo: 撤销后重做");
    Document *doc = document_new();
    document_insert_char(doc, 0, 0, 'z', false);
    int row, col;
    document_undo(doc, &row, &col);
    ASSERT_LINE(doc, 0, "");
    document_redo(doc, &row, &col);
    ASSERT_LINE(doc, 0, "z");
    document_free(doc);
}

static void test_undo_break_line(void) {
    TEST_CASE("undo: 撤销换行");
    Document *doc = document_new();
    document_insert_char(doc, 0, 0, 'a', false);
    document_insert_char(doc, 0, 1, 'b', false);
    document_break_line(doc, 0, 1);
    ASSERT_EQ(document_line_count(doc), 2);
    int row, col;
    document_undo(doc, &row, &col);
    ASSERT_EQ(document_line_count(doc), 1);
    ASSERT_LINE(doc, 0, "ab");
    document_free(doc);
}

static void test_save_mark(void) {
    TEST_CASE("modified 与保存点");
    Document *doc = document_new();
    ASSERT_EQ(doc->modified, 0);

    /* 插入 'a'，然后保存 */
    document_insert_char(doc, 0, 0, 'a', false);
    ASSERT_EQ(doc->modified, 1);
    document_mark_saved(doc);             /* save_idx = undo_top = 1 */
    ASSERT_EQ(doc->modified, 0);

    /* 保存后再插入 'b'，此时 modified=1 */
    document_insert_char(doc, 0, 1, 'b', false);
    ASSERT_EQ(doc->modified, 1);

    /* 撤销 'b'：undo_top 回到 save_idx (1) → modified 应清零 */
    int row, col;
    document_undo(doc, &row, &col);
    ASSERT_EQ(doc->modified, 0);         /* 回到保存点，文件未变 */

    /* 再次撤销 'a'：undo_top (0) < save_idx (1) → 文档已偏离保存版本 */
    document_undo(doc, &row, &col);
    ASSERT_EQ(doc->modified, 1);         /* 超过保存点，内容与磁盘不同 */

    document_free(doc);
}

static void test_load_save(void) {
    TEST_CASE("load/save: 写文件再读回内容一致");
    const char *tmpfile = "tests/fixtures/tmp_test.txt";

    Document *doc = document_new();
    int or_, oc;
    document_insert_text(doc, 0, 0, "line1\nline2\nline3", 17, &or_, &oc);
    ASSERT_EQ(document_save(doc, tmpfile), 0);
    document_free(doc);

    Document *doc2 = document_new();
    ASSERT_EQ(document_load(doc2, tmpfile), 0);
    ASSERT_EQ(document_line_count(doc2), 3);
    ASSERT_LINE(doc2, 0, "line1");
    ASSERT_LINE(doc2, 1, "line2");
    ASSERT_LINE(doc2, 2, "line3");
    document_free(doc2);
    (void)or_; (void)oc;
}

/* ================================================================
 * 入口
 * ================================================================ */
void test_document_run(void) {
    printf("\n[模块] document\n");
    test_new();
    test_insert_char();
    test_insert_middle();
    test_delete_char();
    test_backspace();
    test_break_line();
    test_merge_line();
    test_delete_at_line_end();
    test_insert_text();
    test_delete_range();
    test_undo_insert();
    test_undo_delete();
    test_redo();
    test_undo_break_line();
    test_save_mark();
    test_load_save();
}
