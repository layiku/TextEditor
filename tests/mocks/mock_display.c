/*
 * mock_display.c — display 层的 mock 实现
 * 供 viewport/editor 测试使用，避免依赖真实终端。
 * Phase 5：同步 Cell 结构（uint16_t ch + width）和 display_put_cell(uint32_t) 签名。
 */
#ifdef TEST

#include <stdlib.h>
#include <string.h>
#include "../../include/display.h"
#include "../../include/types.h"
#include "../../include/utf8.h"

#define MOCK_ROWS 25
#define MOCK_COLS 80

static Cell g_mock_buf[MOCK_ROWS][MOCK_COLS];
static int  g_mock_rows = MOCK_ROWS;
static int  g_mock_cols = MOCK_COLS;
static int  g_cur_y = 0;
static int  g_cur_x = 0;

/* 测试辅助：获取 (y,x) 处的 Unicode 码点（向外暴露为 uint32_t） */
uint32_t mock_display_get_char(int y, int x) {
    if (y < 0 || y >= MOCK_ROWS || x < 0 || x >= MOCK_COLS) return 0;
    return (uint32_t)g_mock_buf[y][x].ch;
}

uint8_t mock_display_get_attr(int y, int x) {
    if (y < 0 || y >= MOCK_ROWS || x < 0 || x >= MOCK_COLS) return 0;
    return g_mock_buf[y][x].attr;
}

uint8_t mock_display_get_width(int y, int x) {
    if (y < 0 || y >= MOCK_ROWS || x < 0 || x >= MOCK_COLS) return 0;
    return g_mock_buf[y][x].width;
}

/* ---- display.h 接口实现 ---- */

void display_init(void) {
    memset(g_mock_buf, 0, sizeof(g_mock_buf));
}

void display_exit(void) { /* noop */ }

void display_get_size(int *rows, int *cols) {
    if (rows) *rows = g_mock_rows;
    if (cols) *cols = g_mock_cols;
}

void display_put_cell(int y, int x, uint32_t cp, uint8_t attr) {
    if (y < 0 || y >= g_mock_rows || x < 0 || x >= g_mock_cols) return;
    int w = utf8_cp_width(cp);
    g_mock_buf[y][x].ch    = (uint16_t)(cp & 0xFFFF);
    g_mock_buf[y][x].width = (uint8_t)w;
    g_mock_buf[y][x].attr  = attr;
    if (w == 2 && x + 1 < g_mock_cols) {
        g_mock_buf[y][x+1].ch    = (uint16_t)(cp & 0xFFFF);
        g_mock_buf[y][x+1].width = 0;
        g_mock_buf[y][x+1].attr  = attr;
    }
}

void display_fill(int y, int left, int right, uint8_t attr) {
    for (int x = left; x <= right && x < g_mock_cols; x++) {
        g_mock_buf[y][x].ch    = ' ';
        g_mock_buf[y][x].width = 1;
        g_mock_buf[y][x].attr  = attr;
    }
}

void display_put_str(int y, int x, const char *s, uint8_t attr) {
    if (!s) return;
    int cx = x;
    while (*s && cx < g_mock_cols) {
        uint32_t cp;
        int seq = utf8_decode(s, &cp);
        int w   = utf8_cp_width(cp);
        if (cx + w > g_mock_cols) break;
        display_put_cell(y, cx, cp, attr);
        cx += w;
        s  += seq;
    }
}

void display_put_str_n(int y, int x, const char *s, int max_len, uint8_t attr) {
    if (!s) return;
    int cx = x, used = 0;
    while (*s && cx < g_mock_cols && used < max_len) {
        uint32_t cp;
        int seq = utf8_decode(s, &cp);
        int w   = utf8_cp_width(cp);
        if (used + w > max_len) break;
        if (cx + w > g_mock_cols) break;
        display_put_cell(y, cx, cp, attr);
        cx   += w;
        used += w;
        s    += seq;
    }
}

void display_set_cursor(int y, int x) { g_cur_y = y; g_cur_x = x; }
void display_show_cursor(bool show)    { (void)show; }
void display_flush(void)               { /* noop */ }
void display_clear(uint8_t attr) {
    Cell blank = { ' ', 1, attr };
    for (int r = 0; r < g_mock_rows; r++)
        for (int c = 0; c < g_mock_cols; c++)
            g_mock_buf[r][c] = blank;
}
void display_invalidate(void) { /* noop */ }
void display_resize(void)     { /* noop */ }

#endif /* TEST */
