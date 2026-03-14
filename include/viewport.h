/*
 * viewport.h — 视口管理、坐标换算、编辑区渲染
 * 依赖 document（取行内容）、display（写单元格）、config（获取配置）
 */
#pragma once

#include <stdbool.h>
#include "types.h"
#include "document.h"
#include "syntax.h"  /* SyntaxContext — Phase 6 语法高亮集成 */

/* ================================================================
 * 选区（用于渲染高亮，由 editor 层填入）
 * ================================================================ */
typedef struct {
    bool active;       /* 是否有激活的选区 */
    int  r1, c1;       /* 选区起点（规范化后 r1<=r2，相同行时 c1<c2） */
    int  r2, c2;       /* 选区终点 */
} Selection;

/* ================================================================
 * 视口状态
 * ================================================================ */
typedef struct {
    /* 换行模式 */
    WrapMode mode;

    /* 折行模式下：顶部对应的「显示行」编号（跨逻辑行的折行行号，从 0 计） */
    int view_top_display_row;

    /* 横滚模式下：水平偏移列号（可见区域的最左列对应文档的哪一列） */
    int view_left_col;

    /* 编辑区布局 */
    int edit_top;    /* 编辑区第一行的屏幕行号（= 菜单栏高度，通常=1） */
    int edit_rows;   /* 编辑区可见行数 */
    int edit_left;   /* 编辑区左边界屏幕列号（行号宽度之后） */
    int edit_cols;   /* 编辑区可见列数 */

    /* 行号列宽（0 = 不显示行号） */
    int lineno_width;

    /* 折行缓存：display_line_offset[i] = 第 i 行之前累计的显示行数
     * 用于 O(1) 的逻辑↔显示行号换算 */
    int *line_offset_cache;    /* 长度 = doc->line_count + 1 */
    int  cache_doc_line_count; /* 上次构建缓存时的文档行数 */
    bool cache_dirty;          /* 文档内容改变时置 true，渲染前重建 */
} Viewport;

/* ================================================================
 * 生命周期
 * ================================================================ */
void viewport_init(Viewport *vp);
void viewport_free(Viewport *vp);

/* 每次渲染前调用，根据终端尺寸更新布局参数 */
void viewport_update_layout(Viewport *vp, bool show_lineno);

/* ================================================================
 * 坐标换算
 * ================================================================ */
/* 逻辑 (doc_row, doc_col) → 屏幕 (screen_y, screen_x)
 * 返回 true 表示在当前视口内（可见）；返回 false 表示不可见 */
bool viewport_logic_to_screen(const Viewport *vp, const Document *doc,
                               int doc_row, int doc_col,
                               int *screen_y, int *screen_x);

/* 屏幕 (screen_y, screen_x) → 逻辑 (doc_row, doc_col)
 * 返回 true 表示换算有效 */
bool viewport_screen_to_logic(const Viewport *vp, const Document *doc,
                               int screen_y, int screen_x,
                               int *doc_row, int *doc_col);

/* ================================================================
 * 滚动：确保逻辑坐标 (row, col) 在视口内可见
 * ================================================================ */
void viewport_ensure_visible(Viewport *vp, const Document *doc, int row, int col);

/* Ctrl+← / Ctrl+→：横滚模式下水平移动视口（step 为移动列数，负数向左） */
void viewport_scroll_horizontal(Viewport *vp, const Document *doc, int step);

/* ================================================================
 * 模式切换
 * ================================================================ */
void viewport_set_mode(Viewport *vp, const Document *doc, WrapMode mode);
void viewport_toggle_mode(Viewport *vp, const Document *doc);

/* ================================================================
 * 渲染：将文档内容绘制到 display 后备缓冲
 * sel         — 选区，可为 NULL（无选区高亮）
 * syn         — 语法上下文，可为 NULL（不高亮），Phase 6 新增
 * cursor_row/col — 光标逻辑坐标（用于 display_set_cursor）
 * ================================================================ */
void viewport_render(Viewport *vp, const Document *doc,
                     const Selection *sel,
                     const SyntaxContext *syn,
                     int cursor_row, int cursor_col);

/* ================================================================
 * 垂直滚动条（最右列）
 * ================================================================ */
/* 在最右一列绘制垂直滚动条（track + thumb）。
 * 已在 viewport_render 末尾自动调用，外部通常无需单独调用。 */
void viewport_render_scrollbar(const Viewport *vp, const Document *doc);

/* 命中测试：判断屏幕行 sy 是否落在垂直滚动条的何种区域。
 * 返回：-1=未命中，0=thumb 上方（翻页向上），1=thumb（拖拽），2=thumb 下方（翻页向下）
 * 调用方需先确认 sx == cols-1（最右列）。 */
int viewport_scrollbar_hit(const Viewport *vp, const Document *doc, int sy,
                            int *out_thumb_top, int *out_thumb_h);

/* ================================================================
 * 水平滚动条（WRAP_NONE 模式，编辑区最底行）
 *
 * 布局：在 WRAP_NONE 模式下，编辑区最后 1 行（edit_top + edit_rows - 1）
 * 保留给水平滚动条，实际内容只渲染前 edit_rows - 1 行。
 * WRAP_CHAR 模式下此区域不存在（函数立即返回 / 返回 -1）。
 * ================================================================ */
/* 在编辑区底行绘制水平滚动条。由 viewport_render 自动调用。 */
void viewport_render_hscrollbar(const Viewport *vp, const Document *doc);

/* 命中测试：判断屏幕坐标 (sy, sx) 是否落在水平滚动条上。
 * 返回：-1=未命中，0=thumb 左侧（向左翻），1=thumb（拖拽），2=thumb 右侧（向右翻）。
 * out_thumb_left / out_thumb_w 可传 NULL。 */
int viewport_hscrollbar_hit(const Viewport *vp, const Document *doc,
                              int sy, int sx,
                              int *out_thumb_left, int *out_thumb_w);

/* ================================================================
 * 折行缓存
 * ================================================================ */
/* 重建 display_line_offset 缓存（在文档内容变化后调用） */
void viewport_rebuild_cache(Viewport *vp, const Document *doc);

/* 查询：第 row 逻辑行开始的显示行编号 */
int viewport_display_row_of(const Viewport *vp, const Document *doc, int row);

/* 查询：第 row 逻辑行共占多少显示行 */
int viewport_display_lines_of(const Viewport *vp, const Document *doc, int row);
