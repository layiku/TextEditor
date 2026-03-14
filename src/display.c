/*
 * display.c — 双缓冲显示层实现
 * 维护 front/back 两个 Cell 数组；display_flush 时 diff 输出变化单元格。
 * Phase 5：Cell.ch 升级为 uint16_t（BMP Unicode），支持全宽字符双格渲染。
 */
#include <stdlib.h>
#include <string.h>
#include "display.h"
#include "utf8.h"
#include "util.h"
#include "platform/platform.h"

/* ================================================================
 * 内部状态
 * ================================================================ */
static ScreenBuffer g_sb;
static int  g_cur_y = 0;
static int  g_cur_x = 0;
static bool g_show_cursor  = true;
static bool g_force_redraw = false;

static void alloc_buffers(int rows, int cols) {
    int total  = rows * cols;
    g_sb.rows  = rows;
    g_sb.cols  = cols;
    g_sb.front = (Cell*)safe_malloc((size_t)total * sizeof(Cell));
    g_sb.back  = (Cell*)safe_malloc((size_t)total * sizeof(Cell));

    /* 前缓冲初始化为"不存在"，确保首次 flush 全量输出 */
    Cell blank_back  = { ' ', 1, ATTR_NORMAL };
    Cell blank_front = {  0,  0, 0 };
    for (int i = 0; i < total; i++) {
        g_sb.front[i] = blank_front;
        g_sb.back[i]  = blank_back;
    }
}

static void free_buffers(void) {
    free(g_sb.front);
    free(g_sb.back);
    g_sb.front = NULL;
    g_sb.back  = NULL;
}

/* ================================================================
 * 生命周期
 * ================================================================ */

void display_init(void) {
    plat_init();
    int rows, cols;
    plat_get_size(&rows, &cols);
    alloc_buffers(rows, cols);
    g_force_redraw = true;
}

void display_exit(void) {
    free_buffers();
    plat_exit();
}

void display_get_size(int *rows, int *cols) {
    if (rows) *rows = g_sb.rows;
    if (cols) *cols = g_sb.cols;
}

/* ================================================================
 * 写入后备缓冲
 * ================================================================ */

void display_put_cell(int y, int x, uint32_t cp, uint8_t attr) {
    if (y < 0 || y >= g_sb.rows || x < 0 || x >= g_sb.cols) return;

    int w = utf8_cp_width(cp);  /* 1 = 半宽, 2 = 全宽 */

    Cell *cell = &g_sb.back[y * g_sb.cols + x];
    cell->ch    = (uint16_t)(cp & 0xFFFF);
    cell->width = (uint8_t)w;
    cell->attr  = attr;

    /* 全宽字符：在右侧写续格（保存相同码点，供平台层使用 TRAILING_BYTE 标志） */
    if (w == 2 && x + 1 < g_sb.cols) {
        Cell *cont  = &g_sb.back[y * g_sb.cols + x + 1];
        cont->ch    = (uint16_t)(cp & 0xFFFF);  /* 同一码点 */
        cont->width = 0;                         /* 0 = 续格标记 */
        cont->attr  = attr;
    }
}

void display_fill(int y, int left, int right, uint8_t attr) {
    if (y < 0 || y >= g_sb.rows) return;
    if (left < 0) left = 0;
    if (right >= g_sb.cols) right = g_sb.cols - 1;
    for (int x = left; x <= right; x++) {
        Cell *cell = &g_sb.back[y * g_sb.cols + x];
        cell->ch    = ' ';
        cell->width = 1;
        cell->attr  = attr;
    }
}

void display_put_str(int y, int x, const char *s, uint8_t attr) {
    if (!s) return;
    int cx = x;
    while (*s && cx < g_sb.cols) {
        uint32_t cp;
        int seq = utf8_decode(s, &cp);
        int w   = utf8_cp_width(cp);
        if (cx + w > g_sb.cols) break;  /* 超出右边界不渲染不完整的宽字符 */
        display_put_cell(y, cx, cp, attr);
        cx += w;
        s  += seq;
    }
}

void display_put_str_n(int y, int x, const char *s, int max_len, uint8_t attr) {
    if (!s) return;
    int cx = x, cols_used = 0;
    while (*s && cx < g_sb.cols && cols_used < max_len) {
        uint32_t cp;
        int seq = utf8_decode(s, &cp);
        int w   = utf8_cp_width(cp);
        if (cols_used + w > max_len) break;
        if (cx + w > g_sb.cols) break;
        display_put_cell(y, cx, cp, attr);
        cx         += w;
        cols_used  += w;
        s          += seq;
    }
}

void display_set_cursor(int y, int x) {
    g_cur_y = CLAMP(y, 0, g_sb.rows - 1);
    g_cur_x = CLAMP(x, 0, g_sb.cols - 1);
}

void display_show_cursor(bool show) {
    g_show_cursor = show;
}

/* ================================================================
 * 刷新：差量输出变化单元格
 * ================================================================ */

void display_flush(void) {
    int rows = g_sb.rows;
    int cols = g_sb.cols;

    plat_show_cursor(0);

    for (int y = 0; y < rows; y++) {
        int x = 0;
        while (x < cols) {
            Cell *f = &g_sb.front[y * cols + x];
            Cell *b = &g_sb.back[y * cols + x];
            if (!g_force_redraw &&
                f->ch == b->ch && f->attr == b->attr && f->width == b->width) {
                x++;
                continue;
            }
            int start = x;
            while (x < cols) {
                f = &g_sb.front[y * cols + x];
                b = &g_sb.back[y * cols + x];
                if (!g_force_redraw &&
                    f->ch == b->ch && f->attr == b->attr && f->width == b->width)
                    break;
                x++;
            }
            plat_write_cells(y, start, &g_sb.back[y * cols + start], x - start);
            memcpy(&g_sb.front[y * cols + start],
                   &g_sb.back[y * cols + start],
                   (size_t)(x - start) * sizeof(Cell));
        }
    }

    g_force_redraw = false;
    plat_set_cursor(g_cur_y, g_cur_x);
    if (g_show_cursor) plat_show_cursor(1);
}

/* ================================================================
 * 全屏操作
 * ================================================================ */

void display_clear(uint8_t attr) {
    int total = g_sb.rows * g_sb.cols;
    Cell blank = { ' ', 1, attr };
    for (int i = 0; i < total; i++)
        g_sb.back[i] = blank;
}

void display_invalidate(void) { g_force_redraw = true; }

void display_resize(void) {
    free_buffers();
    int rows, cols;
    plat_get_size(&rows, &cols);
    alloc_buffers(rows, cols);
    g_force_redraw = true;
}
