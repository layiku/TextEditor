/*
 * editor.h — 编辑器核心逻辑接口
 * 管理光标、选区、插入/覆盖模式，协调 document/viewport/clipboard/search
 */
#pragma once

#include <stdbool.h>
#include "types.h"
#include "document.h"
#include "viewport.h"
#include "syntax.h"
#include "search.h"

/* ================================================================
 * 编辑器状态
 * ================================================================ */
typedef struct {
    Document *doc;      /* 当前文档 */
    Viewport  vp;       /* 视口 */

    /* 光标（逻辑坐标） */
    int cursor_row;
    int cursor_col;

    /* 选区 */
    Selection sel;      /* 当前选区（sel.active=false 时无选区） */
    int sel_anchor_row; /* 选区锚点（Shift+方向键时固定的起点） */
    int sel_anchor_col;

    /* 编辑模式 */
    bool insert_mode;   /* true=插入，false=覆盖 */

    /* 查找状态 */
    int last_match_row;
    int last_match_col;
    int last_match_len;
    bool has_match;     /* 上次查找是否有结果 */

    /* 显示配置 */
    bool show_lineno;

    /* 需要重绘标志 */
    bool needs_redraw;

    /* Phase 6：语法高亮上下文（可为空 def = 不高亮） */
    SyntaxContext syn_ctx;
} Editor;

/* ================================================================
 * 生命周期
 * ================================================================ */
void editor_init(Editor *ed);
void editor_free(Editor *ed);

/* ================================================================
 * 文件操作
 * ================================================================ */
int  editor_new_file(Editor *ed);
int  editor_open_file(Editor *ed, const char *path);
int  editor_save_file(Editor *ed);
int  editor_save_file_as(Editor *ed, const char *path);

/* ================================================================
 * 光标移动（移动后自动更新视口使光标可见）
 * ================================================================ */
void editor_move_up(Editor *ed, int lines);
void editor_move_down(Editor *ed, int lines);
void editor_move_left(Editor *ed);
void editor_move_right(Editor *ed);
void editor_move_home(Editor *ed);
void editor_move_end(Editor *ed);
void editor_move_doc_start(Editor *ed);
void editor_move_doc_end(Editor *ed);
void editor_page_up(Editor *ed);
void editor_page_down(Editor *ed);

/* 水平滚动视口（仅 WRAP_NONE 有效，step<0=向左，step>0=向右） */
void editor_scroll_horizontal(Editor *ed, int step);

/* 纯视口垂直滚动，不移动光标（lines>0=向下，lines<0=向上） */
void editor_scroll_view(Editor *ed, int lines);

/* ================================================================
 * 选区操作
 * ================================================================ */
/* 清除选区 */
void editor_sel_clear(Editor *ed);

/* 开始/扩展选区（Shift+方向键调用，先确保有锚点，再移动光标） */
void editor_sel_begin(Editor *ed);
void editor_sel_update(Editor *ed);  /* 光标移动后调用，更新 sel 端点 */

/* 选中整个文档 */
void editor_sel_all(Editor *ed);

/* 选中光标所在单词（连续字母/数字/下划线） */
void editor_sel_word(Editor *ed);

/* 选中光标所在整行 */
void editor_sel_line(Editor *ed);

/* 获取选区文本（调用方 free）；无选区返回 NULL */
char* editor_get_sel_text(Editor *ed, int *out_len);

/* ================================================================
 * 文本编辑
 * ================================================================ */
/* 输入可打印 Unicode 字符（码点）；Phase 5 后支持多字节 CJK 等 */
void editor_insert_char(Editor *ed, uint32_t cp);
void editor_enter(Editor *ed);                 /* 回车 */
void editor_backspace(Editor *ed);             /* Backspace */
void editor_delete(Editor *ed);                /* Delete */
void editor_toggle_insert_mode(Editor *ed);    /* Insert 键 */

/* 粘贴剪贴板内容 */
void editor_paste(Editor *ed);

/* 剪切/复制选区到剪贴板 */
void editor_cut(Editor *ed);
void editor_copy(Editor *ed);

/* 撤销/重做 */
void editor_undo(Editor *ed);
void editor_redo(Editor *ed);

/* ================================================================
 * 查找/替换
 * ================================================================ */
/* 打开查找（保存状态，视图不变） */
void editor_find_next(Editor *ed);
void editor_find_prev(Editor *ed);
void editor_replace_current(Editor *ed);
void editor_replace_all(Editor *ed);

/* ================================================================
 * 渲染（由 main 循环调用）
 * ================================================================ */
void editor_render(Editor *ed);

/* ================================================================
 * 辅助查询
 * ================================================================ */
const char* editor_get_filename(const Editor *ed);  /* 返回文件名（不含路径） */
bool        editor_is_modified(const Editor *ed);
