/*
 * main.c — 程序入口、初始化、主事件循环、退出清理
 *
 * 主循环逻辑：
 *   1. 等待输入事件（最多阻塞 50ms）
 *   2. 优先交给菜单系统处理（Alt+键 / 菜单激活状态）
 *   3. 菜单不处理时，交给编辑器处理快捷键和文本输入
 *   4. 如需重绘，清空后备缓冲并依次渲染菜单栏/编辑区/状态栏，然后 flush
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "editor.h"
#include "search.h"
#include "menu.h"
#include "display.h"
#include "input.h"
#include "config.h"
#include "clipboard.h"
#include "util.h"

/* ================================================================
 * 全局状态（仅 main 使用）
 * ================================================================ */
static Editor    g_editor;
static MenuState g_menu;
static bool      g_running = true;

/* ----------------------------------------------------------------
 * 鼠标点击计数（用于双击/三击检测）
 * ---------------------------------------------------------------- */
/* Returns current time in milliseconds (portable via clock()). */
static long ms_now(void) {
    return (long)((clock() * 1000L) / CLOCKS_PER_SEC);
}

#define MULTI_CLICK_MS 400   /* 两次点击间隔阈值（毫秒） */

static long g_last_click_ms = 0;
static int  g_last_click_x  = -1;
static int  g_last_click_y  = -1;
static int  g_click_count   = 0;   /* 1=单击，2=双击，3=三击 */

/* 更新点击计数器；返回本次点击序号（1/2/3）。*/
static int update_click_count(int cx, int cy) {
    long now  = ms_now();
    long diff = now - g_last_click_ms;
    /* 同一位置且时间在阈值内：累计点击数 */
    if (diff <= MULTI_CLICK_MS && cx == g_last_click_x && cy == g_last_click_y) {
        g_click_count++;
        if (g_click_count > 3) g_click_count = 1;
    } else {
        g_click_count = 1;
    }
    g_last_click_ms = now;
    g_last_click_x  = cx;
    g_last_click_y  = cy;
    return g_click_count;
}

/* 滚动条拖拽状态 */
static bool g_scrollbar_dragging = false;
static int  g_scrollbar_drag_thumb_top = 0;   /* 拖拽开始时 thumb_top */
static int  g_scrollbar_drag_start_y   = 0;   /* 拖拽开始时 my */
static int  g_scrollbar_drag_start_top = 0;   /* 拖拽开始时 view_top_display_row */

/* ================================================================
 * 快捷键处理（菜单未激活时）
 * 返回 true 表示事件已处理
 * ================================================================ */
static bool handle_shortcut(const InputEvent *ev) {
    if (ev->type != EVT_KEY) return false;

    int k   = ev->key;
    int mod = ev->mod;

    /* ---- Ctrl 组合键 ---- */
    if (mod == MOD_CTRL) {
        switch (k) {
        case 'N': {   /* Ctrl+N 新建 */
            if (g_editor.doc->modified) {
                int ch = dialog_save_prompt(editor_get_filename(&g_editor));
                if (ch == 0) editor_save_file(&g_editor);
                else if (ch == 2) return true;
            }
            editor_new_file(&g_editor);
            return true;
        }
        case 'O': {   /* Ctrl+O 打开 */
            if (g_editor.doc->modified) {
                int ch = dialog_save_prompt(editor_get_filename(&g_editor));
                if (ch == 0) editor_save_file(&g_editor);
                else if (ch == 2) return true;
            }
            char path[512] = "";
            /* 初始目录：优先从已打开文件的目录，否则使用当前工作目录 */
            char init_dir[512] = "";
            if (g_editor.doc->filepath[0])
                path_get_dir(g_editor.doc->filepath, init_dir, sizeof(init_dir));
            if (dialog_file_pick_open("Open File", init_dir, path, sizeof(path))) {
                if (editor_open_file(&g_editor, path) != 0)
                    dialog_error("Cannot open file!");
            }
            return true;
        }
        case 'S':     /* Ctrl+S 保存 */
            if (!g_editor.doc->filepath[0]) {
                char path[512] = "";
                if (dialog_file_pick_save("Save File", NULL, "Untitle", path, sizeof(path))) {
                    if (editor_save_file_as(&g_editor, path) != 0)
                        dialog_error("Save failed!");
                }
            } else if (editor_save_file(&g_editor) != 0) {
                dialog_error("Save failed!");
            }
            return true;
        case 'Z':     /* Ctrl+Z 撤销 */
            editor_undo(&g_editor); return true;
        case 'Y':     /* Ctrl+Y 重做 */
            editor_redo(&g_editor); return true;
        case 'X':     /* Ctrl+X 剪切 */
            editor_cut(&g_editor); return true;
        case 'C':     /* Ctrl+C 复制 */
            editor_copy(&g_editor); return true;
        case 'V':     /* Ctrl+V 粘贴 */
            editor_paste(&g_editor); return true;
        case 'A':     /* Ctrl+A 全选 */
            editor_sel_all(&g_editor); return true;
        case 'F': {   /* Ctrl+F 查找 */
            char find_buf[256] = "";
            int  options = SEARCH_CASE_SENSITIVE;
            if (dialog_find(find_buf, sizeof(find_buf), &options) && find_buf[0]) {
                search_init(find_buf, search_get_replace_str(), options);
                editor_find_next(&g_editor);
            }
            return true;
        }
        case 'H': {   /* Ctrl+H 替换 */
            char find_buf[256]    = "";
            char replace_buf[256] = "";
            strncpy(find_buf,    search_get_find_str(),    sizeof(find_buf) - 1);
            strncpy(replace_buf, search_get_replace_str(), sizeof(replace_buf) - 1);
            int options = search_get_options();
            if (dialog_replace(find_buf, sizeof(find_buf),
                               replace_buf, sizeof(replace_buf), &options)) {
                search_init(find_buf, replace_buf, options);
                editor_replace_current(&g_editor);
            }
            return true;
        }
        case 'W':     /* Ctrl+W 切换折行/横滚 */
            viewport_toggle_mode(&g_editor.vp, g_editor.doc);
            config_set_wrap_mode(g_editor.vp.mode);
            display_invalidate();
            g_editor.needs_redraw = true;
            return true;
        }
    }

    /* ---- Ctrl+Shift 组合键 ---- */
    if (mod == (MOD_CTRL | MOD_SHIFT)) {
        if (k == 'S') {   /* Ctrl+Shift+S 另存为 */
            char path[512] = "";
            char init_dir[512] = "";
            const char *init_name = "Untitle";
            if (g_editor.doc->filepath[0]) {
                path_get_dir(g_editor.doc->filepath, init_dir, sizeof(init_dir));
                init_name = editor_get_filename(&g_editor);
            }
            if (dialog_file_pick_save("Save As", init_dir, init_name, path, sizeof(path))) {
                if (editor_save_file_as(&g_editor, path) != 0)
                    dialog_error("Save failed!");
            }
            return true;
        }
    }

    /* ---- Alt 组合键 ---- */
    if (mod == MOD_ALT) {
        if (k == 'X' || k == 'x') {   /* Alt+X 退出 */
            if (g_editor.doc->modified) {
                int ch = dialog_save_prompt(editor_get_filename(&g_editor));
                if (ch == 0) editor_save_file(&g_editor);
                else if (ch == 2) return true;
            }
            g_running = false;
            return true;
        }
    }

    /* ---- Ctrl+← / Ctrl+→（横滚模式水平滚动） ---- */
    if (mod == MOD_CTRL && k == KEY_LEFT) {
        editor_scroll_horizontal(&g_editor, -4);
        return true;
    }
    if (mod == MOD_CTRL && k == KEY_RIGHT) {
        editor_scroll_horizontal(&g_editor, 4);
        return true;
    }

    /* ---- 功能键 ---- */
    if (mod == 0) {
        switch (k) {
        case KEY_F1:    dialog_help(); return true;
        case KEY_F2:    /* F2 保存 */
            if (!g_editor.doc->filepath[0]) {
                char path[512] = "";
                if (dialog_file_pick_save("Save File", NULL, "Untitle", path, sizeof(path)))
                    editor_save_file_as(&g_editor, path);
            } else {
                editor_save_file(&g_editor);
            }
            return true;
        case KEY_F3:    editor_find_next(&g_editor); return true;
        }
    }
    if (mod == MOD_SHIFT && k == KEY_F3) {
        editor_find_prev(&g_editor);
        return true;
    }

    return false;
}

/* ================================================================
 * 编辑键处理（可打印字符 + 导航键）
 * ================================================================ */
static void handle_edit_key(const InputEvent *ev) {
    if (ev->type != EVT_KEY) return;

    int k   = ev->key;
    int mod = ev->mod;

    /* Shift+方向键：扩展选区 */
    if (mod == MOD_SHIFT) {
        switch (k) {
        case KEY_UP:    editor_sel_begin(&g_editor); editor_move_up(&g_editor, 1);   editor_sel_update(&g_editor); return;
        case KEY_DOWN:  editor_sel_begin(&g_editor); editor_move_down(&g_editor, 1); editor_sel_update(&g_editor); return;
        case KEY_LEFT:  editor_sel_begin(&g_editor); editor_move_left(&g_editor);    editor_sel_update(&g_editor); return;
        case KEY_RIGHT: editor_sel_begin(&g_editor); editor_move_right(&g_editor);   editor_sel_update(&g_editor); return;
        case KEY_HOME:  editor_sel_begin(&g_editor); editor_move_home(&g_editor);    editor_sel_update(&g_editor); return;
        case KEY_END:   editor_sel_begin(&g_editor); editor_move_end(&g_editor);     editor_sel_update(&g_editor); return;
        }
    }

    /* 普通方向键：清除选区再移动 */
    if (mod == 0) {
        switch (k) {
        case KEY_UP:       editor_sel_clear(&g_editor); editor_move_up(&g_editor, 1);   return;
        case KEY_DOWN:     editor_sel_clear(&g_editor); editor_move_down(&g_editor, 1); return;
        case KEY_LEFT:     editor_sel_clear(&g_editor); editor_move_left(&g_editor);    return;
        case KEY_RIGHT:    editor_sel_clear(&g_editor); editor_move_right(&g_editor);   return;
        case KEY_HOME:     editor_sel_clear(&g_editor); editor_move_home(&g_editor);    return;
        case KEY_END:      editor_sel_clear(&g_editor); editor_move_end(&g_editor);     return;
        case KEY_PGUP:     editor_sel_clear(&g_editor); editor_page_up(&g_editor);      return;
        case KEY_PGDN:     editor_sel_clear(&g_editor); editor_page_down(&g_editor);    return;
        case KEY_INSERT:   editor_toggle_insert_mode(&g_editor);                        return;
        case KEY_BACKSPACE: editor_backspace(&g_editor);                                return;
        case KEY_DELETE:    editor_delete(&g_editor);                                   return;
        case KEY_ENTER:     editor_enter(&g_editor);                                    return;
        case KEY_TAB:       editor_insert_char(&g_editor, '\t');                        return;
        }

    }

    /* Ctrl+Home / Ctrl+End */
    if (mod == MOD_CTRL && k == KEY_HOME) { editor_move_doc_start(&g_editor); return; }
    if (mod == MOD_CTRL && k == KEY_END)  { editor_move_doc_end(&g_editor);   return; }

    /* 可打印 Unicode 字符（无 Ctrl/Alt 修饰；Phase 5 后 k 可以是 BMP 码点） */
    if (mod == 0 && k >= 32) {
        editor_insert_char(&g_editor, (uint32_t)k);
    }
}

/* ================================================================
 * 鼠标事件处理
 * ================================================================ */

/* Scroll-wheel: scroll the viewport without moving the cursor. */
static void handle_scroll(const InputEvent *ev) {
    /* mscroll>0 = 向文档顶部滚动（行号减小），mscroll<0 = 向末尾滚动 */
    editor_scroll_view(&g_editor, -ev->mscroll);
}

/* Right-click context menu: returns action or -1. */
static void handle_right_click(const InputEvent *ev) {
    int choice = dialog_context_menu(ev->my, ev->mx);
    switch (choice) {
    case 0: editor_cut(&g_editor);    break;
    case 1: editor_copy(&g_editor);   break;
    case 2: editor_paste(&g_editor);  break;
    case 3: editor_sel_all(&g_editor);break;
    case 4: {  /* Find Selected: 以当前选区文本为关键词，查找下一处 */
        int sel_len = 0;
        char *sel_text = editor_get_sel_text(&g_editor, &sel_len);
        if (sel_text && sel_len > 0) {
            search_init(sel_text, search_get_replace_str(), search_get_options());
            editor_find_next(&g_editor);
        }
        free(sel_text);
        break;
    }
    default: break;
    }
    g_editor.needs_redraw = true;
}

/* Horizontal scrollbar drag state (WRAP_NONE only). */
static bool g_hscrollbar_dragging   = false;
static int  g_hscroll_drag_start_x  = 0;
static int  g_hscroll_drag_start_left = 0;

/* Left-click / drag in the edit area. */
static void handle_mouse(const InputEvent *ev) {
    int rows, cols;
    display_get_size(&rows, &cols);

    /* ---- 水平滚动条命中（WRAP_NONE 模式，编辑区底行） ---- */
    if (g_editor.vp.mode == WRAP_NONE && ev->mbutton == 0) {
        int hsr = g_editor.vp.edit_top + g_editor.vp.edit_rows - 1;

        if (ev->type == EVT_MOUSE_DOWN && ev->my == hsr) {
            int hit = viewport_hscrollbar_hit(&g_editor.vp, g_editor.doc,
                                               ev->my, ev->mx, NULL, NULL);
            if (hit == 0) {
                /* 点击 thumb 左侧：向左翻半页 */
                editor_scroll_horizontal(&g_editor, -(g_editor.vp.edit_cols / 2));
            } else if (hit == 2) {
                /* 点击 thumb 右侧：向右翻半页 */
                editor_scroll_horizontal(&g_editor, g_editor.vp.edit_cols / 2);
            } else if (hit == 1) {
                /* 开始 thumb 拖拽 */
                g_hscrollbar_dragging     = true;
                g_hscroll_drag_start_x    = ev->mx;
                g_hscroll_drag_start_left = g_editor.vp.view_left_col;
            }
            return;
        }

        if (ev->type == EVT_MOUSE_MOVE && g_hscrollbar_dragging) {
            int dx      = ev->mx - g_hscroll_drag_start_x;
            int ec      = g_editor.vp.edit_cols;
            int n       = document_line_count(g_editor.doc);
            /* 找最大行长 */
            int max_len = 1;
            for (int i = 0; i < n; i++) {
                int l = document_get_line_len(g_editor.doc, i);
                if (l > max_len) max_len = l;
            }
            int max_left = max_len - ec;
            if (max_left > 0 && ec > 0) {
                int new_left = g_hscroll_drag_start_left + dx * max_left / ec;
                if (new_left < 0)        new_left = 0;
                if (new_left > max_left) new_left = max_left;
                g_editor.vp.view_left_col = new_left;
                g_editor.needs_redraw = true;
            }
            return;
        }
    }

    /* ---- 垂直滚动条命中 ---- */
    /* Scrollbar occupies the rightmost column of the edit area. */
    if (ev->mx == cols - 1 && ev->my >= g_editor.vp.edit_top &&
        ev->my < g_editor.vp.edit_top + g_editor.vp.edit_rows) {

        if (ev->type == EVT_MOUSE_DOWN && ev->mbutton == 0) {
            int thumb_top, thumb_h;
            int hit = viewport_scrollbar_hit(&g_editor.vp, g_editor.doc,
                                              ev->my, &thumb_top, &thumb_h);
            if (hit == 0) {
                editor_page_up(&g_editor);
            } else if (hit == 2) {
                editor_page_down(&g_editor);
            } else if (hit == 1) {
                /* Start drag */
                g_scrollbar_dragging       = true;
                g_scrollbar_drag_thumb_top = thumb_top;
                g_scrollbar_drag_start_y   = ev->my;
                g_scrollbar_drag_start_top = g_editor.vp.view_top_display_row;
            }
            return;
        }

        if (ev->type == EVT_MOUSE_MOVE && g_scrollbar_dragging && ev->mbutton == 0) {
            int dy = ev->my - g_scrollbar_drag_start_y;
            int er = g_editor.vp.edit_rows;
            /* Scale drag distance to document lines */
            int total_disp;
            if (g_editor.vp.mode == WRAP_CHAR) {
                if (g_editor.vp.cache_dirty)
                    viewport_rebuild_cache(&g_editor.vp, g_editor.doc);
                total_disp = g_editor.vp.line_offset_cache[
                                 document_line_count(g_editor.doc)];
            } else {
                total_disp = document_line_count(g_editor.doc);
            }
            int max_top = total_disp - er;
            if (max_top > 0 && er > 0) {
                int new_top = g_scrollbar_drag_start_top + dy * max_top / er;
                if (new_top < 0)       new_top = 0;
                if (new_top > max_top) new_top = max_top;
                g_editor.vp.view_top_display_row = new_top;
                g_editor.needs_redraw = true;
            }
            return;
        }
    }

    /* Release: end any drag */
    if (ev->type == EVT_MOUSE_UP) {
        g_scrollbar_dragging   = false;
        g_hscrollbar_dragging  = false;
        return;
    }
    if (ev->type == EVT_MOUSE_MOVE && ev->mbutton != 0) {
        g_scrollbar_dragging  = false;
        g_hscrollbar_dragging = false;
    }

    /* ---- 左键按下 ---- */
    if (ev->type == EVT_MOUSE_DOWN && ev->mbutton == 0) {
        if (ev->my == 0) return;  /* 菜单栏由菜单系统处理 */

        int clicks = update_click_count(ev->mx, ev->my);

        /* 三击：选中整行 */
        if (clicks == 3) {
            int doc_row, doc_col;
            if (viewport_screen_to_logic(&g_editor.vp, g_editor.doc,
                                          ev->my, ev->mx, &doc_row, &doc_col)) {
                g_editor.cursor_row = doc_row;
                g_editor.cursor_col = doc_col;
                document_clamp_pos(g_editor.doc,
                                   &g_editor.cursor_row, &g_editor.cursor_col);
                editor_sel_line(&g_editor);
            }
            return;
        }

        /* 双击：选中单词 */
        if (clicks == 2) {
            int doc_row, doc_col;
            if (viewport_screen_to_logic(&g_editor.vp, g_editor.doc,
                                          ev->my, ev->mx, &doc_row, &doc_col)) {
                g_editor.cursor_row = doc_row;
                g_editor.cursor_col = doc_col;
                document_clamp_pos(g_editor.doc,
                                   &g_editor.cursor_row, &g_editor.cursor_col);
                editor_sel_word(&g_editor);
            }
            return;
        }

        /* 单击：移动光标 */
        int doc_row, doc_col;
        if (viewport_screen_to_logic(&g_editor.vp, g_editor.doc,
                                     ev->my, ev->mx, &doc_row, &doc_col)) {
            editor_sel_clear(&g_editor);
            g_editor.cursor_row = doc_row;
            g_editor.cursor_col = doc_col;
            document_clamp_pos(g_editor.doc,
                               &g_editor.cursor_row, &g_editor.cursor_col);
            viewport_ensure_visible(&g_editor.vp, g_editor.doc,
                                    g_editor.cursor_row, g_editor.cursor_col);
            g_editor.needs_redraw = true;
        }
        return;
    }

    /* ---- 左键拖拽 ---- */
    if (ev->type == EVT_MOUSE_MOVE && ev->mbutton == 0 && !g_scrollbar_dragging) {
        /* 拖拽到编辑区上边界：自动向上滚动 */
        if (ev->my <= g_editor.vp.edit_top && ev->my > 0) {
            editor_scroll_view(&g_editor, -1);
            editor_sel_begin(&g_editor);
            editor_move_up(&g_editor, 1);
            editor_sel_update(&g_editor);
            return;
        }
        /* 拖拽到编辑区下边界：自动向下滚动 */
        if (ev->my >= g_editor.vp.edit_top + g_editor.vp.edit_rows - 1) {
            editor_scroll_view(&g_editor, 1);
            editor_sel_begin(&g_editor);
            editor_move_down(&g_editor, 1);
            editor_sel_update(&g_editor);
            return;
        }

        /* 普通拖拽：扩展选区 */
        int doc_row, doc_col;
        if (viewport_screen_to_logic(&g_editor.vp, g_editor.doc,
                                     ev->my, ev->mx, &doc_row, &doc_col)) {
            editor_sel_begin(&g_editor);
            g_editor.cursor_row = doc_row;
            g_editor.cursor_col = doc_col;
            editor_sel_update(&g_editor);
            g_editor.needs_redraw = true;
        }
    }
}

/* ================================================================
 * 主循环帧渲染
 * ================================================================ */
static void render_frame(void) {
    if (!g_editor.needs_redraw) return;

    int rows, cols;
    display_get_size(&rows, &cols);
    (void)rows; (void)cols;

    display_clear(ATTR_NORMAL);

    /* 1. 菜单栏 */
    menu_render_bar(&g_menu);

    /* 2. 编辑区 */
    editor_render(&g_editor);

    /* 3. 状态栏 */
    render_status_bar(&g_editor);

    /* 4. 下拉菜单（覆盖在编辑区上方） */
    if (g_menu.dropdown_open)
        menu_render_dropdown(&g_menu);

    display_flush();
}

/* ================================================================
 * 程序入口
 * ================================================================ */
int main(int argc, char *argv[]) {
    /* 加载配置 */
    config_load_auto();

    /* 初始化各模块 */
    clipboard_init();
    display_init();
    input_init();
    editor_init(&g_editor);
    menu_init(&g_menu);

    /* 若命令行提供文件名，打开之 */
    if (argc >= 2) {
        if (editor_open_file(&g_editor, argv[1]) != 0) {
            /* 文件不存在则新建同名文件 */
            strncpy(g_editor.doc->filepath, argv[1],
                    sizeof(g_editor.doc->filepath) - 1);
        }
    }

    /* 主事件循环 */
    g_running = true;
    while (g_running) {
        /* 渲染当前帧 */
        render_frame();

        /* 等待事件（最多 50ms，用于未来支持定时刷新） */
        InputEvent ev;
        int got = input_wait_event(&ev, 50);
        if (!got) continue;

        /* 终端尺寸变化 */
        if (ev.type == EVT_RESIZE) {
            display_resize();
            viewport_update_layout(&g_editor.vp, g_editor.show_lineno);
            display_invalidate();
            g_editor.needs_redraw = true;
            continue;
        }

        /* 优先交给菜单处理 */
        bool consumed = menu_handle_event(&g_menu, &g_editor, &ev);
        if (menu_should_exit()) { g_running = false; break; }
        if (consumed) continue;

        /* 快捷键处理 */
        if (handle_shortcut(&ev)) continue;

        /* 编辑键处理 */
        if (ev.type == EVT_KEY) {
            handle_edit_key(&ev);
        } else if (ev.type == EVT_SCROLL) {
            handle_scroll(&ev);
        } else if (ev.type == EVT_MOUSE_DOWN && ev.mbutton == 1) {
            handle_right_click(&ev);  /* 右键：上下文菜单 */
        } else if (ev.type == EVT_MOUSE_DOWN || ev.type == EVT_MOUSE_MOVE ||
                   ev.type == EVT_MOUSE_UP) {
            handle_mouse(&ev);
        }
    }

    /* 退出清理 */
    config_save_auto();
    clipboard_exit();
    display_exit();
    input_exit();
    editor_free(&g_editor);
    return 0;
}
