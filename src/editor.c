/*
 * editor.c — 编辑器核心逻辑实现
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "editor.h"
#include "clipboard.h"
#include "config.h"
#include "display.h"
#include "utf8.h"
#include "util.h"

/* ================================================================
 * 内部辅助
 * ================================================================ */

static void after_move(Editor *ed) {
    document_clamp_pos(ed->doc, &ed->cursor_row, &ed->cursor_col);
    viewport_ensure_visible(&ed->vp, ed->doc, ed->cursor_row, ed->cursor_col);
    ed->needs_redraw = true;
}

/* Invalidates viewport cache because line lengths may have changed. */
static void after_edit(Editor *ed) {
    ed->vp.cache_dirty = true;
    document_clamp_pos(ed->doc, &ed->cursor_row, &ed->cursor_col);
    viewport_ensure_visible(&ed->vp, ed->doc, ed->cursor_row, ed->cursor_col);
    /* Phase 6：从当前行起增量重建块注释状态 */
    if (ed->syn_ctx.def) {
        syntax_rebuild_state(&ed->syn_ctx, ed->doc, ed->cursor_row);
    }
    ed->needs_redraw = true;
}

/* Ensures r1,c1 is always the earlier position (anchor may be after cursor). */
static void normalize_sel(Selection *sel) {
    if (!sel->active) return;
    int r1 = sel->r1, c1 = sel->c1;
    int r2 = sel->r2, c2 = sel->c2;
    if (r1 > r2 || (r1 == r2 && c1 > c2)) {
        sel->r1 = r2; sel->c1 = c2;
        sel->r2 = r1; sel->c2 = c1;
    }
}

/* ================================================================
 * 生命周期
 * ================================================================ */

void editor_init(Editor *ed) {
    memset(ed, 0, sizeof(Editor));
    ed->doc         = document_new();
    viewport_init(&ed->vp);
    ed->insert_mode = true;
    ed->show_lineno = config_get_show_lineno();
    ed->needs_redraw = true;
    viewport_update_layout(&ed->vp, ed->show_lineno);
    viewport_set_mode(&ed->vp, ed->doc, config_get_wrap_mode());
    syntax_ctx_init(&ed->syn_ctx);  /* Phase 6：初始化语法上下文（def=NULL 表示不高亮） */
}

void editor_free(Editor *ed) {
    document_free(ed->doc);
    viewport_free(&ed->vp);
    syntax_ctx_free(&ed->syn_ctx);  /* Phase 6：释放语法资源 */
    ed->doc = NULL;
}

/* ================================================================
 * 文件操作
 * ================================================================ */

int editor_new_file(Editor *ed) {
    document_clear(ed->doc);
    ed->doc->filepath[0] = '\0';
    ed->cursor_row = 0;
    ed->cursor_col = 0;
    ed->vp.view_top_display_row = 0;
    ed->vp.view_left_col = 0;
    ed->vp.cache_dirty = true;
    editor_sel_clear(ed);
    ed->needs_redraw = true;
    return 0;
}

int editor_open_file(Editor *ed, const char *path) {
    int ret = document_load(ed->doc, path);
    if (ret == 0) {
        ed->cursor_row = 0;
        ed->cursor_col = 0;
        ed->vp.view_top_display_row = 0;
        ed->vp.view_left_col = 0;
        ed->vp.cache_dirty = true;
        editor_sel_clear(ed);
        config_add_recent(path);
        ed->needs_redraw = true;

        /* Phase 6：按文件扩展名自动加载语法定义，初始化块注释状态 */
        syntax_ctx_free(&ed->syn_ctx);
        syntax_ctx_init(&ed->syn_ctx);
        ed->syn_ctx.def = syntax_match_ext(path, NULL);  /* NULL=从当前目录查找 syntax/ */
        if (ed->syn_ctx.def) {
            syntax_rebuild_state(&ed->syn_ctx, ed->doc, -1); /* 全量重建 */
        }
    }
    return ret;
}

int editor_save_file(Editor *ed) {
    if (!ed->doc->filepath[0]) return -1;  /* 需调用 save_as */
    return document_save(ed->doc, ed->doc->filepath);
}

int editor_save_file_as(Editor *ed, const char *path) {
    return document_save(ed->doc, path);
}

/* ================================================================
 * 光标移动
 * ================================================================ */

void editor_move_up(Editor *ed, int lines) {
    ed->cursor_row -= lines;
    if (ed->cursor_row < 0) ed->cursor_row = 0;
    int max_col = document_get_line_len(ed->doc, ed->cursor_row);
    if (ed->cursor_col > max_col) ed->cursor_col = max_col;
    after_move(ed);
}

void editor_move_down(Editor *ed, int lines) {
    int total = document_line_count(ed->doc);
    ed->cursor_row += lines;
    if (ed->cursor_row >= total) ed->cursor_row = total - 1;
    int max_col = document_get_line_len(ed->doc, ed->cursor_row);
    if (ed->cursor_col > max_col) ed->cursor_col = max_col;
    after_move(ed);
}

void editor_move_left(Editor *ed) {
    if (ed->cursor_col > 0) {
        const char *line = document_get_line(ed->doc, ed->cursor_row);
        /* Phase 5：按码点步进，跳过多字节序列 */
        ed->cursor_col = utf8_prev_char(line, ed->cursor_col);
    } else if (ed->cursor_row > 0) {
        /* DOS 标准行为：行首向上跳到上一行行尾 */
        ed->cursor_row--;
        ed->cursor_col = document_get_line_len(ed->doc, ed->cursor_row);
    }
    after_move(ed);
}

void editor_move_right(Editor *ed) {
    int line_len = document_get_line_len(ed->doc, ed->cursor_row);
    if (ed->cursor_col < line_len) {
        const char *line = document_get_line(ed->doc, ed->cursor_row);
        /* Phase 5：按码点步进 */
        ed->cursor_col = utf8_next_char(line, line_len, ed->cursor_col);
    } else if (ed->cursor_row < document_line_count(ed->doc) - 1) {
        /* DOS 标准行为：行尾向下跳到下一行行首 */
        ed->cursor_row++;
        ed->cursor_col = 0;
    }
    after_move(ed);
}

void editor_move_home(Editor *ed) {
    ed->cursor_col = 0;
    after_move(ed);
}

void editor_move_end(Editor *ed) {
    ed->cursor_col = document_get_line_len(ed->doc, ed->cursor_row);
    after_move(ed);
}

void editor_move_doc_start(Editor *ed) {
    ed->cursor_row = 0;
    ed->cursor_col = 0;
    after_move(ed);
}

void editor_move_doc_end(Editor *ed) {
    int total = document_line_count(ed->doc);
    ed->cursor_row = total - 1;
    ed->cursor_col = document_get_line_len(ed->doc, ed->cursor_row);
    after_move(ed);
}

void editor_page_up(Editor *ed) {
    int page = ed->vp.edit_rows > 1 ? ed->vp.edit_rows - 1 : 1;
    editor_move_up(ed, page);
}

void editor_page_down(Editor *ed) {
    int page = ed->vp.edit_rows > 1 ? ed->vp.edit_rows - 1 : 1;
    editor_move_down(ed, page);
}

void editor_scroll_horizontal(Editor *ed, int step) {
    viewport_scroll_horizontal(&ed->vp, ed->doc, step);
    ed->needs_redraw = true;
}

void editor_scroll_view(Editor *ed, int lines) {
    Viewport *vp = &ed->vp;
    int total_disp;
    if (vp->mode == WRAP_CHAR) {
        if (vp->cache_dirty) viewport_rebuild_cache(vp, ed->doc);
        total_disp = vp->line_offset_cache[document_line_count(ed->doc)];
    } else {
        total_disp = document_line_count(ed->doc);
    }
    int max_top = total_disp - vp->edit_rows;
    if (max_top < 0) max_top = 0;
    vp->view_top_display_row += lines;
    if (vp->view_top_display_row > max_top) vp->view_top_display_row = max_top;
    if (vp->view_top_display_row < 0)       vp->view_top_display_row = 0;
    ed->needs_redraw = true;
}

/* ================================================================
 * 选区操作
 * ================================================================ */

void editor_sel_clear(Editor *ed) {
    ed->sel.active = false;
    ed->needs_redraw = true;
}

void editor_sel_begin(Editor *ed) {
    if (!ed->sel.active) {
        /* 第一次 Shift+方向键：设锚点为当前光标 */
        ed->sel_anchor_row = ed->cursor_row;
        ed->sel_anchor_col = ed->cursor_col;
        ed->sel.active = true;
    }
}

void editor_sel_update(Editor *ed) {
    if (!ed->sel.active) return;
    /* 锚点 → 当前光标 形成选区（规范化 r1<=r2） */
    ed->sel.r1 = ed->sel_anchor_row;
    ed->sel.c1 = ed->sel_anchor_col;
    ed->sel.r2 = ed->cursor_row;
    ed->sel.c2 = ed->cursor_col;
    normalize_sel(&ed->sel);
    ed->needs_redraw = true;
}

void editor_sel_all(Editor *ed) {
    int total = document_line_count(ed->doc);
    ed->sel.active = true;
    ed->sel.r1 = 0; ed->sel.c1 = 0;
    ed->sel.r2 = total - 1;
    ed->sel.c2 = document_get_line_len(ed->doc, total - 1);
    ed->sel_anchor_row = 0;
    ed->sel_anchor_col = 0;
    ed->cursor_row = ed->sel.r2;
    ed->cursor_col = ed->sel.c2;
    ed->needs_redraw = true;
}

void editor_sel_word(Editor *ed) {
    const char *line = document_get_line(ed->doc, ed->cursor_row);
    int len = document_get_line_len(ed->doc, ed->cursor_row);
    int col = ed->cursor_col;
    if (col >= len) col = (len > 0) ? utf8_prev_char(line, len) : 0;

    /* Phase 5：选词时按码点边界移动（向左跳过整个多字节序列） */
    int start = col, end = col;
    if (col < len) {
        uint32_t cp;
        utf8_decode(line + col, &cp);
        if (isalnum((unsigned char)line[col]) || line[col] == '_') {
            /* ASCII 字母/数字/下划线：扩展到完整的单词 */
            while (start > 0) {
                int prev = utf8_prev_char(line, start);
                if (!isalnum((unsigned char)line[prev]) && line[prev] != '_') break;
                start = prev;
            }
            while (end < len) {
                if (!isalnum((unsigned char)line[end]) && line[end] != '_') break;
                end = utf8_next_char(line, len, end);
            }
        } else {
            /* 非字母数字（含 CJK）：选中单个字符（码点） */
            end = utf8_next_char(line, len, col);
        }
    }
    ed->sel.active     = true;
    ed->sel.r1         = ed->cursor_row; ed->sel.c1 = start;
    ed->sel.r2         = ed->cursor_row; ed->sel.c2 = end;
    ed->sel_anchor_row = ed->cursor_row; ed->sel_anchor_col = start;
    ed->cursor_col     = end;
    ed->needs_redraw   = true;
}

void editor_sel_line(Editor *ed) {
    int line_len = document_get_line_len(ed->doc, ed->cursor_row);
    ed->sel.active     = true;
    ed->sel.r1         = ed->cursor_row; ed->sel.c1 = 0;
    ed->sel.r2         = ed->cursor_row; ed->sel.c2 = line_len;
    ed->sel_anchor_row = ed->cursor_row; ed->sel_anchor_col = 0;
    ed->cursor_col     = line_len;
    ed->needs_redraw   = true;
}

char* editor_get_sel_text(Editor *ed, int *out_len) {
    if (!ed->sel.active) { if (out_len) *out_len = 0; return NULL; }

    Selection *s = &ed->sel;
    /* 计算大致长度并分配 */
    int approx = 0;
    for (int r = s->r1; r <= s->r2; r++)
        approx += document_get_line_len(ed->doc, r) + 1;

    char *buf = (char*)safe_malloc((size_t)(approx + 2));
    int   bi  = 0;

    for (int r = s->r1; r <= s->r2; r++) {
        const char *line = document_get_line(ed->doc, r);
        int line_len     = document_get_line_len(ed->doc, r);
        int c_start = (r == s->r1) ? s->c1 : 0;
        int c_end   = (r == s->r2) ? s->c2 : line_len;
        if (c_end > line_len) c_end = line_len;

        for (int c = c_start; c < c_end; c++)
            buf[bi++] = line[c];

        if (r < s->r2)
            buf[bi++] = '\n';
    }
    buf[bi] = '\0';
    if (out_len) *out_len = bi;
    return buf;
}

/* ================================================================
 * 文本编辑
 * ================================================================ */

/* 若有选区，先删除选区内容；返回是否确实删了 */
static bool delete_selection(Editor *ed) {
    if (!ed->sel.active) return false;
    Selection *s = &ed->sel;

    char trash[1];  /* 丢弃被删内容 */
    document_delete_range(ed->doc, s->r1, s->c1, s->r2, s->c2,
                          trash, 0);

    /* 光标移到选区起点 */
    ed->cursor_row = s->r1;
    ed->cursor_col = s->c1;
    editor_sel_clear(ed);
    return true;
}

void editor_insert_char(Editor *ed, uint32_t cp) {
    delete_selection(ed);

    /* Phase 5：将 Unicode 码点编码为 UTF-8，插入 1-4 字节 */
    char utf8buf[4];
    int seq = utf8_encode(cp, utf8buf);

    if (ed->insert_mode) {
        document_insert_text(ed->doc, ed->cursor_row, ed->cursor_col,
                             utf8buf, seq,
                             &ed->cursor_row, &ed->cursor_col);
    } else {
        /* 覆盖模式：先删除光标处的一个码点，再插入 */
        int line_len = document_get_line_len(ed->doc, ed->cursor_row);
        if (ed->cursor_col < line_len) {
            document_delete_char(ed->doc, ed->cursor_row, ed->cursor_col);
        }
        int out_row, out_col;
        document_insert_text(ed->doc, ed->cursor_row, ed->cursor_col,
                             utf8buf, seq, &out_row, &out_col);
        ed->cursor_row = out_row;
        ed->cursor_col = out_col;
    }
    after_edit(ed);
}

void editor_enter(Editor *ed) {
    delete_selection(ed);
    document_break_line(ed->doc, ed->cursor_row, ed->cursor_col);
    ed->cursor_row++;
    ed->cursor_col = 0;
    after_edit(ed);
}

void editor_backspace(Editor *ed) {
    if (ed->sel.active) {
        delete_selection(ed);
        after_edit(ed);
        return;
    }
    int old_row = ed->cursor_row;
    int old_col = ed->cursor_col;
    document_backspace(ed->doc, ed->cursor_row, ed->cursor_col);

    if (old_col > 0) {
        ed->cursor_col--;
    } else if (old_row > 0) {
        ed->cursor_row--;
        ed->cursor_col = document_get_line_len(ed->doc, ed->cursor_row);
    }
    after_edit(ed);
}

void editor_delete(Editor *ed) {
    if (ed->sel.active) {
        delete_selection(ed);
        after_edit(ed);
        return;
    }
    document_delete_char(ed->doc, ed->cursor_row, ed->cursor_col);
    after_edit(ed);
}

void editor_toggle_insert_mode(Editor *ed) {
    ed->insert_mode = !ed->insert_mode;
    ed->needs_redraw = true;
}

void editor_paste(Editor *ed) {
    if (clipboard_is_empty()) return;
    delete_selection(ed);

    int clip_len;
    const char *clip = clipboard_get(&clip_len);
    int new_row, new_col;
    document_insert_text(ed->doc, ed->cursor_row, ed->cursor_col,
                         clip, clip_len, &new_row, &new_col);
    ed->cursor_row = new_row;
    ed->cursor_col = new_col;
    after_edit(ed);
}

void editor_cut(Editor *ed) {
    if (!ed->sel.active) return;
    int len;
    char *text = editor_get_sel_text(ed, &len);
    if (text) {
        clipboard_set(text, len);
        free(text);
    }
    delete_selection(ed);
    after_edit(ed);
}

void editor_copy(Editor *ed) {
    if (!ed->sel.active) return;
    int len;
    char *text = editor_get_sel_text(ed, &len);
    if (text) {
        clipboard_set(text, len);
        free(text);
    }
    /* 不修改文档，不需要 after_edit */
    ed->needs_redraw = true;
}

void editor_undo(Editor *ed) {
    int new_row, new_col;
    if (document_undo(ed->doc, &new_row, &new_col) == 0) {
        ed->cursor_row = new_row;
        ed->cursor_col = new_col;
        editor_sel_clear(ed);
        after_edit(ed);
    }
}

void editor_redo(Editor *ed) {
    int new_row, new_col;
    if (document_redo(ed->doc, &new_row, &new_col) == 0) {
        ed->cursor_row = new_row;
        ed->cursor_col = new_col;
        editor_sel_clear(ed);
        after_edit(ed);
    }
}

/* ================================================================
 * 查找/替换
 * ================================================================ */

void editor_find_next(Editor *ed) {
    int row, col;
    /* 从光标当前位置后一个字符开始，避免停在同一处 */
    int from_col = ed->cursor_col + (ed->has_match ? ed->last_match_len : 1);
    int from_row = ed->cursor_row;
    if (from_col > document_get_line_len(ed->doc, from_row)) {
        from_row++;
        from_col = 0;
    }
    int match_len = search_next(ed->doc, from_row, from_col, &row, &col);
    if (match_len > 0) {
        ed->cursor_row    = row;
        ed->cursor_col    = col;
        ed->last_match_row = row;
        ed->last_match_col = col;
        ed->last_match_len = match_len;
        ed->has_match      = true;

        /* 选中匹配区域 */
        ed->sel.active = true;
        ed->sel.r1 = row; ed->sel.c1 = col;
        ed->sel.r2 = row; ed->sel.c2 = col + match_len;
        ed->sel_anchor_row = row;
        ed->sel_anchor_col = col;

        after_move(ed);
    }
}

void editor_find_prev(Editor *ed) {
    int row, col;
    int match_len = search_prev(ed->doc, ed->cursor_row, ed->cursor_col, &row, &col);
    if (match_len > 0) {
        ed->cursor_row    = row;
        ed->cursor_col    = col;
        ed->last_match_row = row;
        ed->last_match_col = col;
        ed->last_match_len = match_len;
        ed->has_match      = true;

        ed->sel.active = true;
        ed->sel.r1 = row; ed->sel.c1 = col;
        ed->sel.r2 = row; ed->sel.c2 = col + match_len;
        ed->sel_anchor_row = row;
        ed->sel_anchor_col = col;

        after_move(ed);
    }
}

void editor_replace_current(Editor *ed) {
    if (!ed->has_match) return;
    search_replace_current(ed->doc, ed->last_match_row, ed->last_match_col,
                           ed->last_match_len);
    ed->has_match = false;
    editor_sel_clear(ed);
    after_edit(ed);
    /* 自动查找下一处 */
    editor_find_next(ed);
}

void editor_replace_all(Editor *ed) {
    int count = search_replace_all(ed->doc);
    ed->has_match = false;
    editor_sel_clear(ed);
    after_edit(ed);
    (void)count;
}

/* ================================================================
 * 渲染
 * ================================================================ */

void editor_render(Editor *ed) {
    viewport_update_layout(&ed->vp, ed->show_lineno);
    /* Phase 6：将语法上下文传入渲染器（def=NULL 时不高亮） */
    const SyntaxContext *syn = ed->syn_ctx.def ? &ed->syn_ctx : NULL;
    viewport_render(&ed->vp, ed->doc, ed->sel.active ? &ed->sel : NULL,
                    syn, ed->cursor_row, ed->cursor_col);
    ed->needs_redraw = false;
}

/* ================================================================
 * 辅助查询
 * ================================================================ */

const char* editor_get_filename(const Editor *ed) {
    const char *path = ed->doc->filepath;
    if (!path[0]) return "Untitle";
    /* 返回文件名部分（最后一个 / 或 \ 之后） */
    const char *name = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') name = p + 1;
    }
    return name;
}

bool editor_is_modified(const Editor *ed) {
    return ed->doc->modified;
}
