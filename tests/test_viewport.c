/*
 * test_viewport.c — viewport 坐标换算单元测试
 * 使用 mock_display（不需要真实终端）
 */
#include <stdio.h>
#include "test_runner.h"
#include "../include/viewport.h"
#include "../include/document.h"
#include "../include/display.h"

/* ---- 构建固定宽度视口（edit_cols=10，无行号，1行菜单+1行状态栏） ---- */
static void init_vp_wrap(Viewport *vp, Document *doc, int edit_cols) {
    viewport_init(vp);
    vp->mode         = WRAP_CHAR;
    vp->edit_top     = 1;
    vp->edit_rows    = 23;
    vp->edit_left    = 0;
    vp->edit_cols    = edit_cols;
    vp->lineno_width = 0;
    vp->view_top_display_row = 0;
    viewport_rebuild_cache(vp, doc);
}

static void init_vp_nowrap(Viewport *vp, Document *doc, int edit_cols) {
    viewport_init(vp);
    vp->mode             = WRAP_NONE;
    vp->edit_top         = 1;
    vp->edit_rows        = 23;
    vp->edit_left        = 0;
    vp->edit_cols        = edit_cols;
    vp->lineno_width     = 0;
    vp->view_top_display_row = 0;  /* 作为 view_top_row 使用 */
    vp->view_left_col    = 0;
}

/* ================================================================
 * 折行模式坐标换算
 * ================================================================ */

static void test_wrap_logic_to_screen_short(void) {
    TEST_CASE("WRAP_CHAR: 短行 logic→screen");
    Document *doc = document_new();
    int or_, oc;
    document_insert_text(doc, 0, 0, "hello", 5, &or_, &oc);

    Viewport vp;
    init_vp_wrap(&vp, doc, 10);

    int sy, sx;
    bool visible = viewport_logic_to_screen(&vp, doc, 0, 3, &sy, &sx);
    ASSERT_EQ(visible, 1);
    ASSERT_EQ(sy, 1);  /* edit_top=1，第一逻辑行=第一显示行 */
    ASSERT_EQ(sx, 3);

    viewport_free(&vp);
    document_free(doc);
    (void)or_; (void)oc;
}

static void test_wrap_logic_to_screen_wrapped(void) {
    TEST_CASE("WRAP_CHAR: 长行折行后坐标换算");
    /* 行长 11，edit_cols=10 → 折成 2 显示行 */
    Document *doc = document_new();
    int or_, oc;
    document_insert_text(doc, 0, 0, "12345678901", 11, &or_, &oc);

    Viewport vp;
    init_vp_wrap(&vp, doc, 10);

    /* (0, 5) 在第一显示行，screen_col=5 */
    int sy, sx;
    viewport_logic_to_screen(&vp, doc, 0, 5, &sy, &sx);
    ASSERT_EQ(sy, 1);
    ASSERT_EQ(sx, 5);

    /* (0, 10) 折到第二显示行，screen_col=0 */
    viewport_logic_to_screen(&vp, doc, 0, 10, &sy, &sx);
    ASSERT_EQ(sy, 2);
    ASSERT_EQ(sx, 0);

    viewport_free(&vp);
    document_free(doc);
    (void)or_; (void)oc;
}

static void test_wrap_screen_to_logic(void) {
    TEST_CASE("WRAP_CHAR: screen→logic 反向换算");
    Document *doc = document_new();
    int or_, oc;
    document_insert_text(doc, 0, 0, "12345678901", 11, &or_, &oc);

    Viewport vp;
    init_vp_wrap(&vp, doc, 10);

    int doc_row, doc_col;
    /* screen(1,0) → (0,0) */
    viewport_screen_to_logic(&vp, doc, 1, 0, &doc_row, &doc_col);
    ASSERT_EQ(doc_row, 0);
    ASSERT_EQ(doc_col, 0);

    /* screen(2,0) → (0,10) */
    viewport_screen_to_logic(&vp, doc, 2, 0, &doc_row, &doc_col);
    ASSERT_EQ(doc_row, 0);
    ASSERT_EQ(doc_col, 10);

    viewport_free(&vp);
    document_free(doc);
    (void)or_; (void)oc;
}

/* ================================================================
 * 横滚模式坐标换算
 * ================================================================ */

static void test_nowrap_logic_to_screen(void) {
    TEST_CASE("WRAP_NONE: logic→screen（view_left_col=5）");
    Document *doc = document_new();
    int or_, oc;
    document_insert_text(doc, 0, 0, "0123456789ABCD", 14, &or_, &oc);

    Viewport vp;
    init_vp_nowrap(&vp, doc, 10);
    vp.view_left_col = 5;

    int sy, sx;
    /* doc_col=7 → screen_col = 7-5 = 2 */
    viewport_logic_to_screen(&vp, doc, 0, 7, &sy, &sx);
    ASSERT_EQ(sy, 1);
    ASSERT_EQ(sx, 2);

    /* doc_col=4 不可见（在 view_left_col=5 左侧） */
    bool vis = viewport_logic_to_screen(&vp, doc, 0, 4, &sy, &sx);
    ASSERT_EQ(vis, 0);

    viewport_free(&vp);
    document_free(doc);
    (void)or_; (void)oc;
}

static void test_nowrap_screen_to_logic(void) {
    TEST_CASE("WRAP_NONE: screen→logic（view_left_col=5）");
    Document *doc = document_new();
    int or_, oc;
    document_insert_text(doc, 0, 0, "0123456789ABCD", 14, &or_, &oc);

    Viewport vp;
    init_vp_nowrap(&vp, doc, 10);
    vp.view_left_col = 5;

    int doc_row, doc_col;
    /* screen(1, 2) → doc (0, 7) */
    viewport_screen_to_logic(&vp, doc, 1, 2, &doc_row, &doc_col);
    ASSERT_EQ(doc_row, 0);
    ASSERT_EQ(doc_col, 7);

    viewport_free(&vp);
    document_free(doc);
    (void)or_; (void)oc;
}

/* ================================================================
 * 折行缓存
 * ================================================================ */

static void test_display_lines_of(void) {
    TEST_CASE("display_lines_of: 空行=1，长行=多行");
    Document *doc = document_new();
    /* 第 0 行：空行，折行=1 */
    /* 第 1 行：11 字符，edit_cols=10 → 折行=2 */
    int or_, oc;
    document_break_line(doc, 0, 0);
    document_insert_text(doc, 1, 0, "12345678901", 11, &or_, &oc);

    Viewport vp;
    init_vp_wrap(&vp, doc, 10);

    ASSERT_EQ(viewport_display_lines_of(&vp, doc, 0), 1);
    ASSERT_EQ(viewport_display_lines_of(&vp, doc, 1), 2);

    viewport_free(&vp);
    document_free(doc);
    (void)or_; (void)oc;
}

/* ================================================================
 * 滚动条命中测试
 * ================================================================ */

/* 用 WRAP_NONE 模式构造一个"小视口 + 大文档"以便精确测试。
 * edit_rows=5, 20 行文档 → thumb_h=1, max_top=15。 */
static void init_scrollbar_vp(Viewport *vp, Document *doc) {
    viewport_init(vp);
    vp->mode      = WRAP_NONE;
    vp->edit_top  = 1;
    vp->edit_rows = 5;
    vp->edit_left = 0;
    vp->edit_cols = 10;
    vp->view_top_display_row = 0;

    /* 插入 20 行内容 */
    for (int i = 0; i < 19; i++) {
        document_break_line(doc, i, document_get_line_len(doc, i));
    }
}

static void test_scrollbar_no_scroll_needed(void) {
    TEST_CASE("scrollbar_hit: 内容 <= 视口高度时始终返回 -1");

    Document *doc = document_new();
    Viewport  vp;
    viewport_init(&vp);
    vp.mode = WRAP_NONE; vp.edit_top = 1; vp.edit_rows = 5;
    vp.edit_cols = 10;
    /* doc 只有 1 行（默认），完全可见 */

    /* 任意行都应返回 -1 */
    ASSERT_EQ(viewport_scrollbar_hit(&vp, doc, 1, NULL, NULL), -1);
    ASSERT_EQ(viewport_scrollbar_hit(&vp, doc, 3, NULL, NULL), -1);

    viewport_free(&vp);
    document_free(doc);
}

static void test_scrollbar_hit_on_thumb(void) {
    TEST_CASE("scrollbar_hit: 点击 thumb 区域 → 返回 1（拖拽）");

    Document *doc = document_new();
    Viewport  vp;
    init_scrollbar_vp(&vp, doc);
    /* view_top=0 → thumb_top=0, thumb_h=1 → rel=0 命中 thumb */
    vp.view_top_display_row = 0;

    int hit = viewport_scrollbar_hit(&vp, doc, vp.edit_top + 0, NULL, NULL);
    ASSERT_EQ(hit, 1);

    viewport_free(&vp);
    document_free(doc);
}

static void test_scrollbar_hit_above_thumb(void) {
    TEST_CASE("scrollbar_hit: 点击 thumb 上方 → 返回 0（向上翻页）");

    Document *doc = document_new();
    Viewport  vp;
    init_scrollbar_vp(&vp, doc);
    /* view_top=7 → thumb_top = 7*(5-1)/15 = 1
     * 点击 rel=0 → 上方 → return 0 */
    vp.view_top_display_row = 7;

    int hit = viewport_scrollbar_hit(&vp, doc, vp.edit_top + 0, NULL, NULL);
    ASSERT_EQ(hit, 0);

    viewport_free(&vp);
    document_free(doc);
}

static void test_scrollbar_hit_below_thumb(void) {
    TEST_CASE("scrollbar_hit: 点击 thumb 下方 → 返回 2（向下翻页）");

    Document *doc = document_new();
    Viewport  vp;
    init_scrollbar_vp(&vp, doc);
    /* view_top=0 → thumb_top=0, thumb_h=1
     * 点击 rel=2 → 下方 → return 2 */
    vp.view_top_display_row = 0;

    int hit = viewport_scrollbar_hit(&vp, doc, vp.edit_top + 2, NULL, NULL);
    ASSERT_EQ(hit, 2);

    viewport_free(&vp);
    document_free(doc);
}

static void test_scrollbar_hit_out_of_range(void) {
    TEST_CASE("scrollbar_hit: 点击编辑区外（菜单栏行）→ 返回 -1");

    Document *doc = document_new();
    Viewport  vp;
    init_scrollbar_vp(&vp, doc);
    vp.view_top_display_row = 5;

    /* sy=0 在菜单栏，rel = 0-1 = -1 → 超界 → -1 */
    ASSERT_EQ(viewport_scrollbar_hit(&vp, doc, 0, NULL, NULL), -1);

    viewport_free(&vp);
    document_free(doc);
}

/* ================================================================
 * 水平滚动条命中测试
 *
 * 固定参数：edit_top=1, edit_rows=5, edit_left=0, edit_cols=10
 *   h-scrollbar row (hsr) = edit_top + edit_rows - 1 = 5
 * 文档：一行 40 个字符，max_line_len=40
 *   → thumb_w = 10*10/40 = 2, max_left = 30
 *   view_left=0  → thumb_left=0  (thumb at col 0-1)
 *   view_left=15 → thumb_left=15*8/30=4 (thumb at col 4-5)
 * ================================================================ */

static void init_hscrollbar_vp(Viewport *vp, Document *doc) {
    viewport_init(vp);
    vp->mode          = WRAP_NONE;
    vp->edit_top      = 1;
    vp->edit_rows     = 5;
    vp->edit_left     = 0;
    vp->edit_cols     = 10;
    vp->view_left_col = 0;

    /* 插入一行 40 字符长的文本，令 max_line_len = 40 */
    int or_, oc;
    document_insert_text(doc, 0, 0,
                          "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMN", 40,
                          &or_, &oc);
    (void)or_; (void)oc;
}

static void test_hscrollbar_wrap_char_mode(void) {
    TEST_CASE("hscrollbar_hit: WRAP_CHAR 模式 → 始终返回 -1");

    Document *doc = document_new();
    Viewport  vp;
    init_hscrollbar_vp(&vp, doc);
    vp.mode = WRAP_CHAR;   /* 改回折行模式 */

    /* 任何坐标都应返回 -1 */
    ASSERT_EQ(viewport_hscrollbar_hit(&vp, doc, 5, 5, NULL, NULL), -1);

    viewport_free(&vp);
    document_free(doc);
}

static void test_hscrollbar_no_hscroll_needed(void) {
    TEST_CASE("hscrollbar_hit: 行宽 <= 视口宽，无需横向滚动 → 返回 -1");

    Document *doc = document_new();
    Viewport  vp;
    viewport_init(&vp);
    vp.mode      = WRAP_NONE;
    vp.edit_top  = 1; vp.edit_rows = 5;
    vp.edit_left = 0; vp.edit_cols = 10;
    /* 默认空行 → max_line_len=1 ≤ edit_cols=10 */

    int hsr = vp.edit_top + vp.edit_rows - 1;  /* = 5 */
    ASSERT_EQ(viewport_hscrollbar_hit(&vp, doc, hsr, 5, NULL, NULL), -1);

    viewport_free(&vp);
    document_free(doc);
}

static void test_hscrollbar_hit_on_thumb(void) {
    TEST_CASE("hscrollbar_hit: view_left=0 → thumb at col 0-1，点击 col 0 → 返回 1");

    Document *doc = document_new();
    Viewport  vp;
    init_hscrollbar_vp(&vp, doc);
    vp.view_left_col = 0;

    int hsr = vp.edit_top + vp.edit_rows - 1;  /* = 5 */
    /* sx = edit_left + 0 = 0, rel = 0 → thumb [0,2) → on thumb */
    ASSERT_EQ(viewport_hscrollbar_hit(&vp, doc, hsr, 0, NULL, NULL), 1);

    viewport_free(&vp);
    document_free(doc);
}

static void test_hscrollbar_hit_right_of_thumb(void) {
    TEST_CASE("hscrollbar_hit: view_left=0 → thumb at col 0-1，点击 col 5 → 返回 2（右侧翻页）");

    Document *doc = document_new();
    Viewport  vp;
    init_hscrollbar_vp(&vp, doc);
    vp.view_left_col = 0;

    int hsr = vp.edit_top + vp.edit_rows - 1;
    /* rel=5 ≥ thumb_left+thumb_w (0+2=2) → 2 */
    ASSERT_EQ(viewport_hscrollbar_hit(&vp, doc, hsr, 5, NULL, NULL), 2);

    viewport_free(&vp);
    document_free(doc);
}

static void test_hscrollbar_hit_left_of_thumb(void) {
    TEST_CASE("hscrollbar_hit: view_left=15 → thumb at col 4-5，点击 col 2 → 返回 0（左侧翻页）");

    Document *doc = document_new();
    Viewport  vp;
    init_hscrollbar_vp(&vp, doc);
    /* view_left=15 → thumb_left = 15*(10-2)/30 = 15*8/30 = 4 */
    vp.view_left_col = 15;

    int hsr = vp.edit_top + vp.edit_rows - 1;
    /* rel=2 < thumb_left(4) → 0 */
    ASSERT_EQ(viewport_hscrollbar_hit(&vp, doc, hsr, 2, NULL, NULL), 0);

    viewport_free(&vp);
    document_free(doc);
}

static void test_hscrollbar_wrong_row(void) {
    TEST_CASE("hscrollbar_hit: sy 不在滚动条行 → 返回 -1");

    Document *doc = document_new();
    Viewport  vp;
    init_hscrollbar_vp(&vp, doc);

    /* hsr=5，点击 sy=3（普通内容行） */
    ASSERT_EQ(viewport_hscrollbar_hit(&vp, doc, 3, 5, NULL, NULL), -1);

    viewport_free(&vp);
    document_free(doc);
}

void test_viewport_run(void) {
    display_init();  /* 使用 mock_display */
    printf("\n[模块] viewport\n");
    test_wrap_logic_to_screen_short();
    test_wrap_logic_to_screen_wrapped();
    test_wrap_screen_to_logic();
    test_nowrap_logic_to_screen();
    test_nowrap_screen_to_logic();
    test_display_lines_of();
    /* 垂直滚动条 */
    test_scrollbar_no_scroll_needed();
    test_scrollbar_hit_on_thumb();
    test_scrollbar_hit_above_thumb();
    test_scrollbar_hit_below_thumb();
    test_scrollbar_hit_out_of_range();
    /* 水平滚动条 */
    test_hscrollbar_wrap_char_mode();
    test_hscrollbar_no_hscroll_needed();
    test_hscrollbar_hit_on_thumb();
    test_hscrollbar_hit_right_of_thumb();
    test_hscrollbar_hit_left_of_thumb();
    test_hscrollbar_wrong_row();
}
