/*
 * test_editor.c — 编辑器核心功能单元测试
 * 覆盖本次新增的鼠标相关 API：
 *   editor_scroll_view / editor_sel_word / editor_sel_line
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_runner.h"
#include "../include/editor.h"
#include "../include/search.h"
#include "../include/display.h"

/* ================================================================
 * 内部辅助：创建带多行内容的 Editor
 * ================================================================ */

/* 在 doc 第 row 行追加字符串（假设该行已存在或可 break 出来）。
 * 简单包装 document_insert_text，忽略输出行列。 */
static void insert_line(Document *doc, int row, const char *text) {
    int or_, oc;
    document_insert_text(doc, row, 0, text, (int)strlen(text), &or_, &oc);
    (void)or_; (void)oc;
}

/* ================================================================
 * editor_scroll_view — 纯视口滚动（光标不动）
 * ================================================================ */

static void test_scroll_view_basic(void) {
    TEST_CASE("scroll_view: 向下/向上滚动视口，光标位置不变");

    Editor ed;
    editor_init(&ed);

    /* 插入 30 行（超过默认 23 行的编辑区高度） */
    for (int i = 0; i < 29; i++) {
        int or_, oc;
        document_break_line(ed.doc, i, document_get_line_len(ed.doc, i));
        (void)or_; (void)oc;
    }
    /* 固定视口为 5 行（便于精确计算） */
    ed.vp.edit_rows = 5;
    ed.vp.mode      = WRAP_NONE;
    ed.vp.view_top_display_row = 0;
    ed.vp.cache_dirty = false;

    int orig_row = ed.cursor_row;
    int orig_col = ed.cursor_col;

    /* 向下滚 3 行 */
    editor_scroll_view(&ed, 3);
    ASSERT_EQ(ed.vp.view_top_display_row, 3);
    /* 光标不动 */
    ASSERT_EQ(ed.cursor_row, orig_row);
    ASSERT_EQ(ed.cursor_col, orig_col);

    /* 再向上滚 2 行 */
    editor_scroll_view(&ed, -2);
    ASSERT_EQ(ed.vp.view_top_display_row, 1);

    editor_free(&ed);
}

static void test_scroll_view_clamp(void) {
    TEST_CASE("scroll_view: 边界夹紧（不超出文档顶/底）");

    Editor ed;
    editor_init(&ed);

    /* 10 行文档 */
    for (int i = 0; i < 9; i++) {
        document_break_line(ed.doc, i, document_get_line_len(ed.doc, i));
    }
    ed.vp.edit_rows = 5;
    ed.vp.mode      = WRAP_NONE;
    ed.vp.view_top_display_row = 3;
    ed.vp.cache_dirty = false;

    /* 向上滚超过 0 → 应夹紧到 0 */
    editor_scroll_view(&ed, -10);
    ASSERT_EQ(ed.vp.view_top_display_row, 0);

    /* 向下滚超过最大值（10-5=5）→ 应夹紧到 5 */
    editor_scroll_view(&ed, 100);
    ASSERT_EQ(ed.vp.view_top_display_row, 5);

    editor_free(&ed);
}

static void test_scroll_view_small_doc(void) {
    TEST_CASE("scroll_view: 文档比视口小时滚动无效果");

    Editor ed;
    editor_init(&ed);
    /* doc 只有 1 行，edit_rows=5 → max_top=0，任何滚动都应钳到 0 */
    ed.vp.edit_rows = 5;
    ed.vp.mode      = WRAP_NONE;
    ed.vp.view_top_display_row = 0;
    ed.vp.cache_dirty = false;

    editor_scroll_view(&ed, 3);
    ASSERT_EQ(ed.vp.view_top_display_row, 0);

    editor_free(&ed);
}

/* ================================================================
 * editor_sel_word — 双击选词
 * ================================================================ */

static void test_sel_word_alnum(void) {
    TEST_CASE("sel_word: 光标在单词中间 → 选中整个单词");

    Editor ed;
    editor_init(&ed);
    insert_line(ed.doc, 0, "hello world");

    ed.cursor_row = 0;
    ed.cursor_col = 2;  /* 在 "hello" 中间 */
    editor_sel_word(&ed);

    ASSERT_EQ(ed.sel.active, 1);
    ASSERT_EQ(ed.sel.r1, 0);
    ASSERT_EQ(ed.sel.c1, 0);   /* "hello" 从第 0 列开始 */
    ASSERT_EQ(ed.sel.r2, 0);
    ASSERT_EQ(ed.sel.c2, 5);   /* "hello" 长度 = 5 */

    editor_free(&ed);
}

static void test_sel_word_at_boundary(void) {
    TEST_CASE("sel_word: 光标在单词末尾（空格前）→ 选中该单词");

    Editor ed;
    editor_init(&ed);
    insert_line(ed.doc, 0, "hello world");

    ed.cursor_row = 0;
    ed.cursor_col = 6;  /* 指向 "world" 的 'w' */
    editor_sel_word(&ed);

    ASSERT_EQ(ed.sel.active, 1);
    ASSERT_EQ(ed.sel.c1, 6);   /* "world" 从第 6 列 */
    ASSERT_EQ(ed.sel.c2, 11);  /* 长度 = 5 → 结束在 11 */

    editor_free(&ed);
}

static void test_sel_word_punctuation(void) {
    TEST_CASE("sel_word: 光标在非单词字符（标点）→ 选中单个字符");

    Editor ed;
    editor_init(&ed);
    insert_line(ed.doc, 0, "foo+bar");

    ed.cursor_row = 0;
    ed.cursor_col = 3;  /* '+' */
    editor_sel_word(&ed);

    ASSERT_EQ(ed.sel.active, 1);
    ASSERT_EQ(ed.sel.c1, 3);
    ASSERT_EQ(ed.sel.c2, 4);   /* 单字符选区 */

    editor_free(&ed);
}

static void test_sel_word_with_underscore(void) {
    TEST_CASE("sel_word: 下划线视为单词字符（C 标识符）");

    Editor ed;
    editor_init(&ed);
    insert_line(ed.doc, 0, "my_var = 1");

    ed.cursor_row = 0;
    ed.cursor_col = 4;  /* 在 "my_var" 中 */
    editor_sel_word(&ed);

    ASSERT_EQ(ed.sel.active, 1);
    ASSERT_EQ(ed.sel.c1, 0);
    ASSERT_EQ(ed.sel.c2, 6);   /* "my_var" 长度 = 6 */

    editor_free(&ed);
}

static void test_sel_word_empty_line(void) {
    TEST_CASE("sel_word: 空行上调用 → sel 激活但范围为空");

    Editor ed;
    editor_init(&ed);
    /* doc 默认有一个空行，直接测试 */
    ed.cursor_row = 0;
    ed.cursor_col = 0;
    editor_sel_word(&ed);

    ASSERT_EQ(ed.sel.active, 1);
    /* 空行：start=end=0，选区宽度=0 */
    ASSERT_EQ(ed.sel.c1, 0);
    ASSERT_EQ(ed.sel.c2, 0);

    editor_free(&ed);
}

/* ================================================================
 * editor_sel_line — 三击选行
 * ================================================================ */

static void test_sel_line_with_content(void) {
    TEST_CASE("sel_line: 光标在有内容的行 → 选中全行");

    Editor ed;
    editor_init(&ed);
    insert_line(ed.doc, 0, "hello");

    ed.cursor_row = 0;
    ed.cursor_col = 2;
    editor_sel_line(&ed);

    ASSERT_EQ(ed.sel.active, 1);
    ASSERT_EQ(ed.sel.r1, 0);
    ASSERT_EQ(ed.sel.c1, 0);
    ASSERT_EQ(ed.sel.r2, 0);
    ASSERT_EQ(ed.sel.c2, 5);   /* 整行长度 = 5 */
    ASSERT_EQ(ed.cursor_col, 5);

    editor_free(&ed);
}

static void test_sel_line_middle_of_doc(void) {
    TEST_CASE("sel_line: 选中多行文档中的第 2 行");

    Editor ed;
    editor_init(&ed);
    insert_line(ed.doc, 0, "line1");
    document_break_line(ed.doc, 0, 5);
    insert_line(ed.doc, 1, "line2");

    ed.cursor_row = 1;
    ed.cursor_col = 0;
    editor_sel_line(&ed);

    ASSERT_EQ(ed.sel.active, 1);
    ASSERT_EQ(ed.sel.r1, 1);
    ASSERT_EQ(ed.sel.c1, 0);
    ASSERT_EQ(ed.sel.r2, 1);
    ASSERT_EQ(ed.sel.c2, 5);

    editor_free(&ed);
}

static void test_sel_line_empty(void) {
    TEST_CASE("sel_line: 空行 → 选区宽度为 0，仍然激活");

    Editor ed;
    editor_init(&ed);
    ed.cursor_row = 0;
    ed.cursor_col = 0;
    editor_sel_line(&ed);

    ASSERT_EQ(ed.sel.active, 1);
    ASSERT_EQ(ed.sel.c1, 0);
    ASSERT_EQ(ed.sel.c2, 0);

    editor_free(&ed);
}

/* ================================================================
 * Find Selected 关联流程
 * 模拟"双击选词 → 右键 Find Selected → 跳到下一处"
 * ================================================================ */

static void test_find_selected_basic(void) {
    TEST_CASE("find_selected: 选区文本作为关键词，editor_find_next 跳到下一处");

    Editor ed;
    editor_init(&ed);

    /* 文档：第 0 行 "hello world hello"，第一个 "hello" 后还有一处 */
    insert_line(ed.doc, 0, "hello world hello");

    /* 手动建立与双击等效的选区：选中第一个 "hello" */
    ed.cursor_row = 0;
    ed.cursor_col = 0;
    editor_sel_word(&ed);  /* 选中 col 0-5 的 "hello" */
    ASSERT_EQ(ed.sel.c1, 0);
    ASSERT_EQ(ed.sel.c2, 5);

    /* 获取选区文本并初始化搜索（模拟 handle_right_click case 4） */
    int sel_len = 0;
    char *sel_text = editor_get_sel_text(&ed, &sel_len);
    ASSERT_NOT_NULL(sel_text);
    ASSERT_EQ(sel_len, 5);

    search_init(sel_text, "", SEARCH_CASE_SENSITIVE);
    free(sel_text);

    /* 从当前光标（col 5）之后开始查找下一个 "hello" */
    editor_find_next(&ed);

    /* 第二个 "hello" 从 col 12 开始 */
    ASSERT_EQ(ed.cursor_row, 0);
    ASSERT_EQ(ed.cursor_col, 12);
    ASSERT_EQ(ed.has_match,  1);

    editor_free(&ed);
}

static void test_find_selected_no_match(void) {
    TEST_CASE("find_selected: 选区文本在文档中不存在 → has_match = 0，光标不跳");

    Editor ed;
    editor_init(&ed);
    insert_line(ed.doc, 0, "hello world");

    /* 手动构造一个"选区文本"，但该文本在文档中根本不存在 */
    int saved_row = ed.cursor_row;
    int saved_col = ed.cursor_col;
    search_init("zzz", "", SEARCH_CASE_SENSITIVE);

    editor_find_next(&ed);
    /* 搜索 "zzz" 在文档中不存在 → has_match 应为 false，光标不变 */
    ASSERT_EQ(ed.has_match,   0);
    ASSERT_EQ(ed.cursor_row,  saved_row);
    ASSERT_EQ(ed.cursor_col,  saved_col);

    editor_free(&ed);
}

static void test_find_selected_case_insensitive(void) {
    TEST_CASE("find_selected: 大小写不敏感选项对 find_selected 同样生效");

    Editor ed;
    editor_init(&ed);
    insert_line(ed.doc, 0, "Hello hello HELLO");

    /* 选中第一个 "Hello" */
    ed.cursor_row = 0; ed.cursor_col = 0;
    editor_sel_word(&ed);

    int sel_len = 0;
    char *sel_text = editor_get_sel_text(&ed, &sel_len);
    ASSERT_NOT_NULL(sel_text);
    /* 使用大小写不敏感选项 */
    search_init(sel_text, "", 0);  /* 0 = 不设 SEARCH_CASE_SENSITIVE */
    free(sel_text);

    editor_find_next(&ed);
    /* 下一处大小写不敏感匹配在 col 6（"hello"） */
    ASSERT_EQ(ed.has_match,  1);
    ASSERT_EQ(ed.cursor_col, 6);

    editor_free(&ed);
}

/* ================================================================
 * 测试入口
 * ================================================================ */

/* ================================================================
 * editor_get_filename — Phase 8：无文件时返回 "Untitle"
 * ================================================================ */

static void test_get_filename_untitle(void) {
    TEST_CASE("editor_get_filename: 无文件时返回 \"Untitle\"");
    Editor ed;
    editor_init(&ed);
    /* 新建文档，filepath 为空 */
    ASSERT_STR_EQ(editor_get_filename(&ed), "Untitle");
    editor_free(&ed);
}

static void test_get_filename_with_path(void) {
    TEST_CASE("editor_get_filename: 有路径时返回文件名部分");
    Editor ed;
    editor_init(&ed);
    /* 直接写 filepath，不实际打开文件 */
    strncpy(ed.doc->filepath, "/home/user/hello.c",
            sizeof(ed.doc->filepath) - 1);
    ASSERT_STR_EQ(editor_get_filename(&ed), "hello.c");
    editor_free(&ed);
}

void test_editor_run(void) {
    display_init();
    printf("\n[模块] editor\n");

    test_scroll_view_basic();
    test_scroll_view_clamp();
    test_scroll_view_small_doc();

    test_sel_word_alnum();
    test_sel_word_at_boundary();
    test_sel_word_punctuation();
    test_sel_word_with_underscore();
    test_sel_word_empty_line();

    test_sel_line_with_content();
    test_sel_line_middle_of_doc();
    test_sel_line_empty();

    test_find_selected_basic();
    test_find_selected_no_match();
    test_find_selected_case_insensitive();

    test_get_filename_untitle();
    test_get_filename_with_path();
}
