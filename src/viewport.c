/*
 * viewport.c — 视口管理与编辑区渲染
 * Phase 5：全面 UTF-8 化。cursor_col 保持字节偏移；view_left_col 改为显示列偏移。
 * 渲染时使用 utf8_decode/utf8_cp_width 解码，宽字符占 2 显示列。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "viewport.h"
#include "display.h"
#include "syntax.h"
#include "utf8.h"
#include "config.h"
#include "util.h"

/* ================================================================
 * 内部：折行宽度计算（Phase 5：基于显示宽度，非字节数）
 * ================================================================ */

/* 计算一行（含 Tab 展开）的总显示宽度 */
static int line_display_width_with_tabs(const char *line, int byte_len) {
    int tab_w = config_get_tab_size();
    int col = 0, pos = 0;
    while (pos < byte_len) {
        uint32_t cp;
        int seq = utf8_decode(line + pos, &cp);
        if (pos + seq > byte_len) break;
        if (cp == '\t') {
            col = ((col / tab_w) + 1) * tab_w;
        } else {
            col += utf8_cp_width(cp);
        }
        pos += seq;
    }
    return col;
}

/* Tab 感知的字节偏移 → 显示列换算 */
static int byte_to_col_tabs(const char *line, int byte_len, int byte_off) {
    int tab_w = config_get_tab_size();
    int col = 0, pos = 0;
    while (pos < byte_len && pos < byte_off) {
        uint32_t cp;
        int seq = utf8_decode(line + pos, &cp);
        if (pos + seq > byte_len) break;
        if (cp == '\t') {
            col = ((col / tab_w) + 1) * tab_w;
        } else {
            col += utf8_cp_width(cp);
        }
        pos += seq;
    }
    return col;
}

/* Tab 感知的显示列 → 字节偏移换算 */
static int col_to_byte_tabs(const char *line, int byte_len, int target_col) {
    int tab_w = config_get_tab_size();
    int col = 0, pos = 0;
    while (pos < byte_len) {
        if (col >= target_col) break;
        uint32_t cp;
        int seq = utf8_decode(line + pos, &cp);
        if (pos + seq > byte_len) break;
        int next_col;
        if (cp == '\t') {
            next_col = ((col / tab_w) + 1) * tab_w;
        } else {
            next_col = col + utf8_cp_width(cp);
        }
        /* target_col 落在宽字符/Tab 展开格内部 → 返回该字符起始 */
        if (next_col > target_col) break;
        col = next_col;
        pos += seq;
    }
    return pos;
}

/* 一行显示宽度 disp_w 在折行模式下占多少显示行（空行=1） */
static int display_lines_for_width(int disp_w, int edit_cols) {
    if (edit_cols <= 0) return 1;
    return (disp_w == 0) ? 1 : (disp_w + edit_cols - 1) / edit_cols;
}

/* ================================================================
 * 生命周期
 * ================================================================ */

void viewport_init(Viewport *vp) {
    memset(vp, 0, sizeof(Viewport));
    vp->mode        = WRAP_CHAR;
    vp->cache_dirty = true;
}

void viewport_free(Viewport *vp) {
    free(vp->line_offset_cache);
    vp->line_offset_cache    = NULL;
    vp->cache_doc_line_count = 0;
}

void viewport_update_layout(Viewport *vp, bool show_lineno) {
    int rows, cols;
    display_get_size(&rows, &cols);

    int menu_h      = 1;
    int status_h    = 1;
    int scrollbar_w = 1;

    vp->lineno_width = show_lineno ? 6 : 0;
    vp->edit_top  = menu_h;
    vp->edit_rows = rows - menu_h - status_h;
    if (vp->edit_rows < 1) vp->edit_rows = 1;

    vp->edit_left = vp->lineno_width;
    vp->edit_cols = cols - vp->lineno_width - scrollbar_w;
    if (vp->edit_cols < 1) vp->edit_cols = 1;
}

/* ================================================================
 * 折行缓存（基于 UTF-8 显示宽度）
 * ================================================================ */

void viewport_rebuild_cache(Viewport *vp, const Document *doc) {
    int n = document_line_count(doc);

    if (vp->line_offset_cache == NULL || vp->cache_doc_line_count < n + 1) {
        free(vp->line_offset_cache);
        vp->line_offset_cache    = (int*)safe_malloc((size_t)(n + 2) * sizeof(int));
        vp->cache_doc_line_count = n + 1;
    }

    int acc = 0;
    for (int i = 0; i < n; i++) {
        vp->line_offset_cache[i] = acc;
        const char *line = document_get_line(doc, i);
        int         blen = document_get_line_len(doc, i);
        int         dw   = line_display_width_with_tabs(line, blen);
        acc += display_lines_for_width(dw, vp->edit_cols);
    }
    vp->line_offset_cache[n] = acc;
    vp->cache_dirty = false;
}

int viewport_display_row_of(const Viewport *vp, const Document *doc, int row) {
    if (!vp->line_offset_cache || vp->cache_dirty)
        viewport_rebuild_cache((Viewport*)vp, doc);
    if (row < 0) return 0;
    if (row >= document_line_count(doc))
        return vp->line_offset_cache[document_line_count(doc)];
    return vp->line_offset_cache[row];
}

int viewport_display_lines_of(const Viewport *vp, const Document *doc, int row) {
    if (!vp->line_offset_cache || vp->cache_dirty)
        viewport_rebuild_cache((Viewport*)vp, doc);
    if (row < 0 || row >= document_line_count(doc)) return 1;
    return vp->line_offset_cache[row + 1] - vp->line_offset_cache[row];
}

/* ================================================================
 * 坐标换算（Phase 5：cursor_col = 字节偏移，view_left_col = 显示列）
 * ================================================================ */

bool viewport_logic_to_screen(const Viewport *vp, const Document *doc,
                               int doc_row, int doc_col,
                               int *screen_y, int *screen_x) {
    const char *line     = document_get_line(doc, doc_row);
    int         line_len = document_get_line_len(doc, doc_row);

    if (vp->mode == WRAP_CHAR) {
        if (vp->cache_dirty) viewport_rebuild_cache((Viewport*)vp, doc);

        /* 字节偏移 → 显示列（Tab 感知） */
        int disp_col = byte_to_col_tabs(line, line_len, doc_col);
        int sub      = (vp->edit_cols > 0) ? (disp_col / vp->edit_cols) : 0;
        int col_in   = (vp->edit_cols > 0) ? (disp_col % vp->edit_cols) : disp_col;

        int disp_row = vp->line_offset_cache[doc_row];
        int sy = vp->edit_top + (disp_row + sub - vp->view_top_display_row);
        int sx = vp->edit_left + col_in;

        if (screen_y) *screen_y = sy;
        if (screen_x) *screen_x = sx;
        return (sy >= vp->edit_top && sy < vp->edit_top + vp->edit_rows);

    } else {
        int view_top_row = vp->view_top_display_row;
        int sy = vp->edit_top + (doc_row - view_top_row);

        /* 字节偏移 → 显示列（Tab 感知），再减去水平偏移 */
        int doc_disp_col = byte_to_col_tabs(line, line_len, doc_col);
        int sx = vp->edit_left + (doc_disp_col - vp->view_left_col);

        if (screen_y) *screen_y = sy;
        if (screen_x) *screen_x = sx;
        return (sy >= vp->edit_top && sy < vp->edit_top + vp->edit_rows &&
                sx >= vp->edit_left && sx < vp->edit_left + vp->edit_cols);
    }
}

bool viewport_screen_to_logic(const Viewport *vp, const Document *doc,
                               int screen_y, int screen_x,
                               int *doc_row, int *doc_col) {
    int ey = screen_y - vp->edit_top;
    int ex = screen_x - vp->edit_left;
    if (ey < 0 || ey >= vp->edit_rows) return false;
    if (ex < 0) ex = 0;

    if (vp->mode == WRAP_CHAR) {
        if (vp->cache_dirty) viewport_rebuild_cache((Viewport*)vp, doc);

        int disp = vp->view_top_display_row + ey;
        int total_lines = document_line_count(doc);

        int lo = 0, hi = total_lines - 1, row = 0;
        while (lo <= hi) {
            int mid   = (lo + hi) / 2;
            int start = vp->line_offset_cache[mid];
            int end   = vp->line_offset_cache[mid + 1] - 1;
            if (disp < start)      { hi = mid - 1; }
            else if (disp > end)   { lo = mid + 1; }
            else                   { row = mid; break; }
        }

        /* 子行内的显示列 → 字节偏移（Tab 感知） */
        int sub          = disp - vp->line_offset_cache[row];
        int disp_col     = sub * vp->edit_cols + ex;
        int line_len     = document_get_line_len(doc, row);
        const char *line = document_get_line(doc, row);
        int col = col_to_byte_tabs(line, line_len, disp_col);
        if (col > line_len) col = line_len;

        if (doc_row) *doc_row = row;
        if (doc_col) *doc_col = col;
        return true;

    } else {
        int view_top_row = vp->view_top_display_row;
        int row = view_top_row + ey;
        int total = document_line_count(doc);
        if (row >= total) row = total - 1;
        if (row < 0)      row = 0;

        /* 屏幕列 ex + 水平偏移 → 文档显示列 → 字节偏移（Tab 感知） */
        int line_len     = document_get_line_len(doc, row);
        const char *line = document_get_line(doc, row);
        int disp_col     = vp->view_left_col + ex;
        int col = col_to_byte_tabs(line, line_len, disp_col);
        if (col < 0)        col = 0;
        if (col > line_len) col = line_len;

        if (doc_row) *doc_row = row;
        if (doc_col) *doc_col = col;
        return true;
    }
}

/* ================================================================
 * 滚动控制
 * ================================================================ */

void viewport_ensure_visible(Viewport *vp, const Document *doc, int row, int col) {
    if (vp->mode == WRAP_CHAR) {
        if (vp->cache_dirty) viewport_rebuild_cache(vp, doc);

        /* 字节偏移 → 显示列（Tab 感知） → 子行 */
        const char *line = document_get_line(doc, row);
        int         blen = document_get_line_len(doc, row);
        int disp_col = byte_to_col_tabs(line, blen, col);
        int sub      = (vp->edit_cols > 0) ? (disp_col / vp->edit_cols) : 0;
        int disp     = vp->line_offset_cache[row] + sub;

        if (disp < vp->view_top_display_row)
            vp->view_top_display_row = disp;
        if (disp >= vp->view_top_display_row + vp->edit_rows)
            vp->view_top_display_row = disp - vp->edit_rows + 1;

    } else {
        int view_top_row = vp->view_top_display_row;

        /* 垂直滚动 */
        if (row < view_top_row)
            vp->view_top_display_row = row;
        if (row >= view_top_row + vp->edit_rows)
            vp->view_top_display_row = row - vp->edit_rows + 1;

        /* 水平滚动（基于显示列，Tab 感知）*/
        const char *line = document_get_line(doc, row);
        int         blen = document_get_line_len(doc, row);
        int disp_col = byte_to_col_tabs(line, blen, col);

        if (disp_col < vp->view_left_col)
            vp->view_left_col = disp_col;
        if (disp_col >= vp->view_left_col + vp->edit_cols)
            vp->view_left_col = disp_col - vp->edit_cols + 1;
        if (vp->view_left_col < 0) vp->view_left_col = 0;
    }
}

void viewport_scroll_horizontal(Viewport *vp, const Document *doc, int step) {
    if (vp->mode != WRAP_NONE) return;
    vp->view_left_col += step;
    if (vp->view_left_col < 0) vp->view_left_col = 0;
    (void)doc;
}

/* ================================================================
 * 模式切换
 * ================================================================ */

void viewport_set_mode(Viewport *vp, const Document *doc, WrapMode mode) {
    if (vp->mode == mode) return;
    vp->mode = mode;
    vp->view_left_col = 0;
    if (mode == WRAP_CHAR) {
        vp->cache_dirty = true;
        viewport_rebuild_cache(vp, doc);
        int total_disp = vp->line_offset_cache[document_line_count(doc)];
        if (vp->view_top_display_row >= total_disp)
            vp->view_top_display_row = total_disp > 0 ? total_disp - 1 : 0;
    }
}

void viewport_toggle_mode(Viewport *vp, const Document *doc) {
    viewport_set_mode(vp, doc, vp->mode == WRAP_CHAR ? WRAP_NONE : WRAP_CHAR);
}

/* ================================================================
 * 渲染辅助
 * ================================================================ */

/* 判断 (row, col) 是否在选区内 */
static bool in_selection(const Selection *sel, int row, int col) {
    if (!sel || !sel->active) return false;
    if (row < sel->r1 || row > sel->r2) return false;
    if (row == sel->r1 && col < sel->c1) return false;
    if (row == sel->r2 && col >= sel->c2) return false;
    return true;
}

/* 渲染一行内容（UTF-8 感知 + 语法高亮）：
 * 从字节偏移 byte_start 开始，渲染最多 ec 个显示列。
 * syn_attrs[i] 为字节 i 的颜色属性；0 表示无特殊高亮。
 * 优先级：选区 > 语法高亮 > ATTR_NORMAL */
static void render_line_content(int sy, int edit_left, int ec,
                                 const Document *doc, const Selection *sel,
                                 const uint8_t *syn_attrs, int syn_len,
                                 int doc_row, int byte_start) {
    const char *line     = document_get_line(doc, doc_row);
    int         line_len = document_get_line_len(doc, doc_row);
    int doc_col   = byte_start;
    int disp_used = 0;

    while (disp_used < ec) {
        if (doc_col >= line_len) {
            uint8_t attr_empty = in_selection(sel, doc_row, doc_col)
                                 ? ATTR_REVERSE : ATTR_NORMAL;
            display_fill(sy, edit_left + disp_used, edit_left + ec - 1, attr_empty);
            break;
        }

        uint32_t cp;
        int seq = utf8_decode(line + doc_col, &cp);
        int w   = utf8_cp_width(cp);

        /* 宽字符无法完整放入剩余空间：填空格停止 */
        if (disp_used + w > ec) {
            display_fill(sy, edit_left + disp_used, edit_left + ec - 1, ATTR_NORMAL);
            break;
        }

        /* 属性优先级：选区 > 语法高亮 > ATTR_NORMAL */
        uint8_t attr;
        if (in_selection(sel, doc_row, doc_col)) {
            attr = ATTR_REVERSE;
        } else if (syn_attrs && doc_col < syn_len && syn_attrs[doc_col]) {
            attr = syn_attrs[doc_col];
        } else {
            attr = ATTR_NORMAL;
        }

        if (cp == '\t') {
            /* Tab 按 tab_size 对齐展开为若干空格 */
            int tab_w    = config_get_tab_size();
            int tab_stop = ((disp_used / tab_w) + 1) * tab_w;
            int spaces   = tab_stop - disp_used;
            if (disp_used + spaces > ec) spaces = ec - disp_used;
            if (spaces > 0)
                display_fill(sy, edit_left + disp_used,
                             edit_left + disp_used + spaces - 1, attr);
            disp_used += spaces;
            doc_col   += seq;
            continue;
        }

        display_put_cell(sy, edit_left + disp_used, cp, attr);
        disp_used += w;
        doc_col   += seq;
    }
}

/* 最大一行字节数（用于 syn_attrs 栈缓冲） */
#define SYNTAX_ATTRS_BUF 4096

void viewport_render(Viewport *vp, const Document *doc,
                     const Selection *sel,
                     const SyntaxContext *syn,
                     int cursor_row, int cursor_col) {
    int cols, total_rows;
    display_get_size(&total_rows, &cols);
    (void)total_rows;

    /* 每行的语法高亮属性数组（按字节索引），栈上分配 */
    uint8_t syn_attrs[SYNTAX_ATTRS_BUF];

    if (vp->mode == WRAP_CHAR) {
        if (vp->cache_dirty) viewport_rebuild_cache(vp, doc);

        int top_disp = vp->view_top_display_row;
        int ec       = vp->edit_cols;
        int n        = document_line_count(doc);

        for (int ey = 0; ey < vp->edit_rows; ey++) {
            int disp = top_disp + ey;
            int sy   = vp->edit_top + ey;

            /* 二分找逻辑行 */
            int lo = 0, hi = n - 1, doc_row = n;
            while (lo <= hi) {
                int mid = (lo + hi) / 2;
                if (vp->line_offset_cache[mid] <= disp &&
                    disp < vp->line_offset_cache[mid + 1]) {
                    doc_row = mid; break;
                }
                if (vp->line_offset_cache[mid] > disp) hi = mid - 1;
                else                                    lo = mid + 1;
            }

            if (doc_row >= n) {
                display_fill(sy, 0, cols - 1, ATTR_NORMAL);
                continue;
            }

            int sub = disp - vp->line_offset_cache[doc_row];
            /* 子行对应的显示列起点 → 字节偏移 */
            int col_disp_start = sub * ec;
            const char *line   = document_get_line(doc, doc_row);
            int         blen   = document_get_line_len(doc, doc_row);
            int byte_start     = utf8_col_to_byte(line, blen, col_disp_start);

            /* 行号列 */
            if (vp->lineno_width > 0) {
                if (sub == 0) {
                    char lnbuf[8];
                    snprintf(lnbuf, sizeof(lnbuf), "%5d ", doc_row + 1);
                    display_put_str_n(sy, 0, lnbuf, vp->lineno_width, ATTR_LINENO);
                } else {
                    display_fill(sy, 0, vp->lineno_width - 1, ATTR_LINENO);
                }
            }

            /* 语法高亮：仅在每逻辑行的首个子行调用，子行共用同一份属性 */
            int syn_len = 0;
            if (syn && sub == 0) {
                int cap = blen < SYNTAX_ATTRS_BUF ? blen : SYNTAX_ATTRS_BUF;
                syntax_highlight_line(syn, doc, doc_row, syn_attrs, cap);
                syn_len = cap;
            }

            /* 文本列（UTF-8 感知 + 语法高亮） */
            render_line_content(sy, vp->edit_left, ec, doc, sel,
                                syn_len > 0 ? syn_attrs : NULL, syn_len,
                                doc_row, byte_start);

            /* 滚动条占位（稍后由 render_scrollbar 覆盖） */
            display_put_cell(sy, cols - 1, ' ', ATTR_SCROLLBAR);
        }

    } else {
        /* 横滚模式：保留最后一行给水平滚动条 */
        int view_top     = vp->view_top_display_row;
        int ec           = vp->edit_cols;
        int n            = document_line_count(doc);
        int content_rows = vp->edit_rows - 1;

        for (int ey = 0; ey < content_rows; ey++) {
            int doc_row = view_top + ey;
            int sy      = vp->edit_top + ey;

            if (doc_row >= n) {
                display_fill(sy, 0, cols - 1, ATTR_NORMAL);
                continue;
            }

            /* 行号 */
            if (vp->lineno_width > 0) {
                char lnbuf[8];
                snprintf(lnbuf, sizeof(lnbuf), "%5d ", doc_row + 1);
                display_put_str_n(sy, 0, lnbuf, vp->lineno_width, ATTR_LINENO);
            }

            /* 语法高亮 */
            int blen   = document_get_line_len(doc, doc_row);
            int syn_len = 0;
            if (syn) {
                int cap = blen < SYNTAX_ATTRS_BUF ? blen : SYNTAX_ATTRS_BUF;
                syntax_highlight_line(syn, doc, doc_row, syn_attrs, cap);
                syn_len = cap;
            }

            /* 水平偏移 view_left_col 是显示列 → 转换为字节偏移开始渲染 */
            const char *line = document_get_line(doc, doc_row);
            int byte_start   = utf8_col_to_byte(line, blen, vp->view_left_col);
            render_line_content(sy, vp->edit_left, ec, doc, sel,
                                syn_len > 0 ? syn_attrs : NULL, syn_len,
                                doc_row, byte_start);

            display_put_cell(sy, cols - 1, ' ', ATTR_SCROLLBAR);
        }
    }

    viewport_render_scrollbar(vp, doc);
    viewport_render_hscrollbar(vp, doc);

    /* 光标位置 */
    int cur_sy, cur_sx;
    if (viewport_logic_to_screen(vp, doc, cursor_row, cursor_col, &cur_sy, &cur_sx)) {
        display_set_cursor(cur_sy, cur_sx);
        display_show_cursor(true);
    } else {
        display_show_cursor(false);
    }
}

/* ================================================================
 * 滚动条渲染与命中测试
 * ================================================================ */

static int calc_scrollbar(const Viewport *vp, const Document *doc,
                           int *out_thumb_top, int *out_thumb_h) {
    int total_disp;
    if (vp->mode == WRAP_CHAR) {
        if (vp->cache_dirty) viewport_rebuild_cache((Viewport*)vp, doc);
        total_disp = vp->line_offset_cache[document_line_count(doc)];
    } else {
        total_disp = document_line_count(doc);
    }
    int er = vp->edit_rows;

    if (total_disp <= er || er <= 0) {
        if (out_thumb_top) *out_thumb_top = 0;
        if (out_thumb_h)   *out_thumb_h   = er;
        return total_disp;
    }

    int th = er * er / total_disp;
    if (th < 1) th = 1;
    if (th > er) th = er;

    int max_top = total_disp - er;
    int top = (max_top > 0)
              ? vp->view_top_display_row * (er - th) / max_top
              : 0;
    if (top > er - th) top = er - th;
    if (top < 0)       top = 0;

    if (out_thumb_top) *out_thumb_top = top;
    if (out_thumb_h)   *out_thumb_h   = th;
    return total_disp;
}

void viewport_render_scrollbar(const Viewport *vp, const Document *doc) {
    int rows, cols;
    display_get_size(&rows, &cols);
    (void)rows;
    int sx = cols - 1;

    int v_rows = (vp->mode == WRAP_NONE) ? vp->edit_rows - 1 : vp->edit_rows;

    int thumb_top, thumb_h;
    int total_disp = calc_scrollbar(vp, doc, &thumb_top, &thumb_h);

    for (int i = 0; i < v_rows; i++) {
        int  sy       = vp->edit_top + i;
        bool in_thumb = (total_disp > v_rows) &&
                        (i >= thumb_top && i < thumb_top + thumb_h);
        display_put_cell(sy, sx,
                         (uint32_t)(in_thumb ? '#' : '|'),
                         in_thumb ? ATTR_SCROLLTHUMB : ATTR_SCROLLBAR);
    }
}

int viewport_scrollbar_hit(const Viewport *vp, const Document *doc, int sy,
                            int *out_thumb_top, int *out_thumb_h) {
    int v_rows = (vp->mode == WRAP_NONE) ? vp->edit_rows - 1 : vp->edit_rows;

    int thumb_top, thumb_h;
    int total_disp = calc_scrollbar(vp, doc, &thumb_top, &thumb_h);
    if (out_thumb_top) *out_thumb_top = thumb_top;
    if (out_thumb_h)   *out_thumb_h   = thumb_h;

    int rel = sy - vp->edit_top;
    if (rel < 0 || rel >= v_rows)    return -1;
    if (total_disp <= v_rows)        return -1;

    if (rel < thumb_top)             return 0;
    if (rel < thumb_top + thumb_h)   return 1;
    return 2;
}

/* ================================================================
 * 水平滚动条（WRAP_NONE 模式）
 * Phase 5：doc_max_line_display_width 返回显示列数，非字节数。
 * ================================================================ */

static int doc_max_line_display_width(const Document *doc) {
    int n = document_line_count(doc);
    int mx = 1;
    for (int i = 0; i < n; i++) {
        const char *line = document_get_line(doc, i);
        int         blen = document_get_line_len(doc, i);
        int         dw   = line_display_width_with_tabs(line, blen);
        if (dw > mx) mx = dw;
    }
    return mx;
}

static int calc_hscrollbar(const Viewport *vp, const Document *doc,
                            int *out_thumb_left, int *out_thumb_w) {
    int total = doc_max_line_display_width(doc);
    int ec    = vp->edit_cols;

    if (total <= ec) {
        if (out_thumb_left) *out_thumb_left = 0;
        if (out_thumb_w)    *out_thumb_w    = ec;
        return total;
    }

    int tw = ec * ec / total;
    if (tw < 1) tw = 1;
    if (tw > ec) tw = ec;

    int max_left = total - ec;
    int tl = (max_left > 0)
             ? vp->view_left_col * (ec - tw) / max_left
             : 0;
    if (tl > ec - tw) tl = ec - tw;
    if (tl < 0)       tl = 0;

    if (out_thumb_left) *out_thumb_left = tl;
    if (out_thumb_w)    *out_thumb_w    = tw;
    return total;
}

void viewport_render_hscrollbar(const Viewport *vp, const Document *doc) {
    if (vp->mode != WRAP_NONE) return;

    int rows, cols;
    display_get_size(&rows, &cols);
    (void)rows;

    int hsr = vp->edit_top + vp->edit_rows - 1;

    int thumb_left, thumb_w;
    int total = calc_hscrollbar(vp, doc, &thumb_left, &thumb_w);

    if (vp->lineno_width > 0)
        display_fill(hsr, 0, vp->edit_left - 1, ATTR_SCROLLBAR);

    for (int i = 0; i < vp->edit_cols; i++) {
        bool in_thumb = (total > vp->edit_cols) &&
                        (i >= thumb_left && i < thumb_left + thumb_w);
        display_put_cell(hsr, vp->edit_left + i,
                         (uint32_t)(in_thumb ? '#' : '='),
                         in_thumb ? ATTR_SCROLLTHUMB : ATTR_SCROLLBAR);
    }

    display_put_cell(hsr, cols - 1, '+', ATTR_SCROLLBAR);
}

int viewport_hscrollbar_hit(const Viewport *vp, const Document *doc,
                             int sy, int sx,
                             int *out_thumb_left, int *out_thumb_w) {
    if (vp->mode != WRAP_NONE) return -1;

    int hsr = vp->edit_top + vp->edit_rows - 1;
    if (sy != hsr)                               return -1;
    if (sx < vp->edit_left ||
        sx >= vp->edit_left + vp->edit_cols)     return -1;

    int thumb_left, thumb_w;
    int total = calc_hscrollbar(vp, doc, &thumb_left, &thumb_w);
    if (out_thumb_left) *out_thumb_left = thumb_left;
    if (out_thumb_w)    *out_thumb_w    = thumb_w;

    if (total <= vp->edit_cols) return -1;

    int rel = sx - vp->edit_left;
    if (rel < thumb_left)             return 0;
    if (rel < thumb_left + thumb_w)   return 1;
    return 2;
}
