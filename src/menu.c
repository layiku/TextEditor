/*
 * menu.c — Menu bar rendering, dropdown navigation, and command dispatch
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "menu.h"
#include "dialog.h"
#include "display.h"
#include "input.h"
#include "search.h"
#include "config.h"
#include "util.h"

/* ================================================================
 * Menu data
 * ================================================================ */

typedef struct {
    const char *label;    /* NULL = separator line */
    const char *shortcut;
    int         cmd_id;
} SubItem;

typedef struct {
    const char    *label;
    int            accel;  /* uppercase letter for Alt+key activation */
    const SubItem *items;
} TopItem;

enum {
    CMD_NONE = 0,
    CMD_NEW, CMD_OPEN, CMD_SAVE, CMD_SAVE_AS, CMD_EXIT,
    CMD_UNDO, CMD_REDO, CMD_CUT, CMD_COPY, CMD_PASTE,
    CMD_SEL_ALL, CMD_DELETE,
    CMD_FIND, CMD_FIND_NEXT, CMD_FIND_PREV, CMD_REPLACE, CMD_REPLACE_ALL,
    CMD_TOGGLE_WRAP, CMD_TOGGLE_LINENO,
    CMD_SET_LANGUAGE,   /* View > Set Language... */
    CMD_HELP,
};

static const SubItem g_file_items[] = {
    { "New",        "Ctrl+N",       CMD_NEW     },
    { "Open...",    "Ctrl+O",       CMD_OPEN    },
    { "Save",       "Ctrl+S",       CMD_SAVE    },
    { "Save As...", "Ctrl+Shift+S", CMD_SAVE_AS },
    { NULL,         NULL,           0           },
    { "Exit",       "Alt+X",        CMD_EXIT    },
    { NULL, NULL, -1 }
};

static const SubItem g_edit_items[] = {
    { "Undo",       "Ctrl+Z", CMD_UNDO     },
    { "Redo",       "Ctrl+Y", CMD_REDO     },
    { NULL,         NULL,     0            },
    { "Cut",        "Ctrl+X", CMD_CUT      },
    { "Copy",       "Ctrl+C", CMD_COPY     },
    { "Paste",      "Ctrl+V", CMD_PASTE    },
    { "Delete",     "Del",    CMD_DELETE   },
    { NULL,         NULL,     0            },
    { "Select All", "Ctrl+A", CMD_SEL_ALL  },
    { NULL, NULL, -1 }
};

static const SubItem g_search_items[] = {
    { "Find...",     "Ctrl+F",   CMD_FIND        },
    { "Find Next",   "F3",       CMD_FIND_NEXT   },
    { "Find Prev",   "Shift+F3", CMD_FIND_PREV   },
    { "Replace...",  "Ctrl+H",   CMD_REPLACE     },
    { "Replace All", "",         CMD_REPLACE_ALL },
    { NULL, NULL, -1 }
};

static const SubItem g_view_items[] = {
    { "Toggle Wrap",    "Ctrl+W", CMD_TOGGLE_WRAP   },
    { "Toggle LineNo",  "",       CMD_TOGGLE_LINENO },
    { "Set Language...", "",      CMD_SET_LANGUAGE  },
    { NULL, NULL, -1 }
};

static const SubItem g_help_items[] = {
    { "Help", "F1", CMD_HELP },
    { NULL, NULL, -1 }
};

#define TOP_ITEM_COUNT 5
static const TopItem g_top_items[TOP_ITEM_COUNT] = {
    { "File(F)",   'F', g_file_items   },
    { "Edit(E)",   'E', g_edit_items   },
    { "Search(S)", 'S', g_search_items },
    { "View(V)",   'V', g_view_items   },
    { "Help(H)",   'H', g_help_items   },
};

/* ================================================================
 * Internal helpers
 * ================================================================ */

/* Minimum width to fit all label+shortcut pairs with padding. */
static int dropdown_width(const SubItem *items) {
    int w = 20;
    for (int i = 0; items[i].cmd_id != -1; i++) {
        if (!items[i].label) continue;
        int total = (int)strlen(items[i].label)
                  + (items[i].shortcut ? (int)strlen(items[i].shortcut) : 0)
                  + 4;
        if (total > w) w = total;
    }
    return w;
}

/* Number of entries excluding the terminator. */
static int dropdown_count(const SubItem *items) {
    int n = 0;
    for (int i = 0; items[i].cmd_id != -1; i++) n++;
    return n;
}

/* ================================================================
 * Lifecycle
 * ================================================================ */

void menu_init(MenuState *ms) {
    memset(ms, 0, sizeof(MenuState));
    ms->top_item = -1;
    ms->sub_item = -1;
}

/* ================================================================
 * Rendering
 * ================================================================ */

void menu_render_bar(const MenuState *ms) {
    int rows, cols;
    display_get_size(&rows, &cols);
    (void)rows;

    display_fill(0, 0, cols - 1, ATTR_MENUBAR);

    int x = 1;
    for (int i = 0; i < TOP_ITEM_COUNT; i++) {
        uint8_t attr = (ms->active && ms->top_item == i)
                       ? ATTR_MENU_SEL : ATTR_MENUBAR;
        display_put_str(0, x, g_top_items[i].label, attr);
        x += (int)strlen(g_top_items[i].label) + 2;
    }
}

void menu_render_dropdown(const MenuState *ms) {
    if (!ms->dropdown_open || ms->top_item < 0) return;

    const TopItem *top   = &g_top_items[ms->top_item];
    const SubItem *items = top->items;
    int            dw    = dropdown_width(items);
    int            dc    = dropdown_count(items);

    int x = 1;
    for (int i = 0; i < ms->top_item; i++)
        x += (int)strlen(g_top_items[i].label) + 2;

    int rows, cols;
    display_get_size(&rows, &cols);
    if (x + dw > cols) x = cols - dw;
    if (x < 0) x = 0;

    int y = 1, item_idx = 0;
    for (int i = 0; i < dc; i++) {
        const SubItem *si = &items[i];
        if (y >= rows - 1) break;

        if (!si->label) {
            for (int j = x; j < x + dw; j++)
                display_put_cell(y, j, '-', ATTR_DIALOG_BG);
        } else {
            uint8_t attr = (ms->sub_item == item_idx) ? ATTR_MENU_SEL : ATTR_DIALOG_BG;
            display_fill(y, x, x + dw - 1, attr);
            display_put_str(y, x + 1, si->label, attr);

            if (si->shortcut && si->shortcut[0]) {
                int sw = (int)strlen(si->shortcut);
                int sx = x + dw - 1 - sw;
                if (sx > x + 1)
                    display_put_str(y, sx, si->shortcut, attr);
            }
            item_idx++;
        }
        y++;
    }
}

/* ================================================================
 * Command execution
 * ================================================================ */

static bool execute_command(int cmd, Editor *ed) {
    switch (cmd) {

    case CMD_NEW:
        if (ed->doc->modified) {
            int choice = dialog_save_prompt(editor_get_filename(ed));
            if (choice == 0) editor_save_file(ed);
            else if (choice == 2) return false;
        }
        editor_new_file(ed);
        break;

    case CMD_OPEN: {
        if (ed->doc->modified) {
            int choice = dialog_save_prompt(editor_get_filename(ed));
            if (choice == 0) editor_save_file(ed);
            else if (choice == 2) return false;
        }
        char path[512] = "";
        char init_dir[512] = "";
        if (ed->doc->filepath[0])
            path_get_dir(ed->doc->filepath, init_dir, sizeof(init_dir));
        if (dialog_file_pick_open("Open File", init_dir, path, sizeof(path))) {
            if (editor_open_file(ed, path) != 0)
                dialog_error("Cannot open file!");
        }
        break;
    }

    case CMD_SAVE:
        if (!ed->doc->filepath[0]) {
            char path[512] = "";
            if (dialog_file_pick_save("Save File", NULL, "Untitle", path, sizeof(path)))
                if (editor_save_file_as(ed, path) != 0)
                    dialog_error("Save failed!");
        } else if (editor_save_file(ed) != 0) {
            dialog_error("Save failed!");
        }
        break;

    case CMD_SAVE_AS: {
        char path[512] = "";
        char init_dir[512] = "";
        const char *init_name = "Untitle";
        if (ed->doc->filepath[0]) {
            path_get_dir(ed->doc->filepath, init_dir, sizeof(init_dir));
            init_name = editor_get_filename(ed);
        }
        if (dialog_file_pick_save("Save As", init_dir, init_name, path, sizeof(path)))
            if (editor_save_file_as(ed, path) != 0)
                dialog_error("Save failed!");
        break;
    }

    case CMD_EXIT:
        if (ed->doc->modified) {
            int choice = dialog_save_prompt(editor_get_filename(ed));
            if (choice == 0) editor_save_file(ed);
            else if (choice == 2) return false;
        }
        return true;

    case CMD_UNDO:    editor_undo(ed);    break;
    case CMD_REDO:    editor_redo(ed);    break;
    case CMD_CUT:     editor_cut(ed);     break;
    case CMD_COPY:    editor_copy(ed);    break;
    case CMD_PASTE:   editor_paste(ed);   break;
    case CMD_DELETE:  editor_delete(ed);  break;
    case CMD_SEL_ALL: editor_sel_all(ed); break;

    case CMD_FIND: {
        char find_buf[256] = "";
        int  options = SEARCH_CASE_SENSITIVE;
        if (dialog_find(find_buf, sizeof(find_buf), &options) && find_buf[0]) {
            search_init(find_buf, search_get_replace_str(), options);
            editor_find_next(ed);
        }
        break;
    }

    case CMD_FIND_NEXT: editor_find_next(ed); break;
    case CMD_FIND_PREV: editor_find_prev(ed); break;

    case CMD_REPLACE: {
        char find_buf[256]    = "";
        char replace_buf[256] = "";
        strncpy(find_buf,    search_get_find_str(),    sizeof(find_buf)    - 1);
        strncpy(replace_buf, search_get_replace_str(), sizeof(replace_buf) - 1);
        int options = search_get_options();
        if (dialog_replace(find_buf, sizeof(find_buf),
                           replace_buf, sizeof(replace_buf), &options)) {
            search_init(find_buf, replace_buf, options);
            editor_replace_current(ed);
        }
        break;
    }

    case CMD_REPLACE_ALL: editor_replace_all(ed); break;

    case CMD_TOGGLE_WRAP:
        viewport_toggle_mode(&ed->vp, ed->doc);
        config_set_wrap_mode(ed->vp.mode);
        display_invalidate();
        ed->needs_redraw = true;
        break;

    case CMD_TOGGLE_LINENO:
        ed->show_lineno = !ed->show_lineno;
        config_set_show_lineno(ed->show_lineno);
        viewport_update_layout(&ed->vp, ed->show_lineno);
        display_invalidate();
        ed->needs_redraw = true;
        break;

    case CMD_SET_LANGUAGE: {
        /* 扫描 exe_dir/syntax/ 目录，列出所有可用语言 */
        char exe_dir[512], syn_dir[600];
        get_exe_dir(exe_dir, sizeof(exe_dir));
        path_join(exe_dir, "syntax", syn_dir, sizeof(syn_dir));

        SyntaxLangInfo langs[SYNTAX_MAX_LANGS];
        int n = syntax_list_languages(syn_dir, langs, SYNTAX_MAX_LANGS);

        /* 构建选项列表：index 0 = 禁用高亮，index 1..n = 各语言 */
        const char *items[SYNTAX_MAX_LANGS + 1];
        items[0] = "(None - disable highlighting)";
        for (int i = 0; i < n; i++) items[i + 1] = langs[i].name;

        int sel = dialog_list_select("Set Language", items, n + 1);
        if (sel < 0) break;  /* Esc 取消，不改变当前设置 */

        /* 释放旧语法定义，按选择加载新定义 */
        syntax_free(ed->syn_ctx.def);
        ed->syn_ctx.def = NULL;
        if (sel > 0) {
            ed->syn_ctx.def = syntax_load(langs[sel - 1].path);
            if (ed->syn_ctx.def)
                syntax_rebuild_state(&ed->syn_ctx, ed->doc, -1);
        }
        display_invalidate();
        ed->needs_redraw = true;
        break;
    }

    case CMD_HELP:
        dialog_help();
        break;

    default: break;
    }
    return false;
}

/* ================================================================
 * Event handling
 * ================================================================ */

static bool g_should_exit = false;
bool menu_should_exit(void) { return g_should_exit; }

bool menu_handle_event(MenuState *ms, Editor *ed, const InputEvent *ev) {
    if (!ms->active) {
        if (ev->type == EVT_KEY && (ev->mod & MOD_ALT)) {
            char c = (char)(ev->key & ~0x20);
            for (int i = 0; i < TOP_ITEM_COUNT; i++) {
                if (c == g_top_items[i].accel) {
                    ms->active = ms->dropdown_open = true;
                    ms->top_item = i;
                    ms->sub_item = 0;
                    ed->needs_redraw = true;
                    return true;
                }
            }
        }
        if (ev->type == EVT_MOUSE_DOWN && ev->my == 0) {
            int x = 1;
            for (int i = 0; i < TOP_ITEM_COUNT; i++) {
                int w = (int)strlen(g_top_items[i].label);
                if (ev->mx >= x && ev->mx < x + w) {
                    ms->active = ms->dropdown_open = true;
                    ms->top_item = i;
                    ms->sub_item = 0;
                    ed->needs_redraw = true;
                    return true;
                }
                x += w + 2;
            }
        }
        return false;
    }

    if (ev->type != EVT_KEY) {
        if (ev->type == EVT_MOUSE_DOWN) {
            /* Click on menu bar row: switch top-level item */
            if (ev->my == 0) {
                int x = 1;
                for (int i = 0; i < TOP_ITEM_COUNT; i++) {
                    int w = (int)strlen(g_top_items[i].label);
                    if (ev->mx >= x && ev->mx < x + w) {
                        if (ms->top_item == i && ms->dropdown_open) {
                            /* Toggle off same item */
                            ms->active = ms->dropdown_open = false;
                            ms->top_item = ms->sub_item = -1;
                        } else {
                            ms->active = ms->dropdown_open = true;
                            ms->top_item = i;
                            ms->sub_item = 0;
                        }
                        ed->needs_redraw = true;
                        return true;
                    }
                    x += w + 2;
                }
            } else if (ms->dropdown_open) {
                /* Click within the open dropdown */
                const SubItem *items = g_top_items[ms->top_item].items;
                int dw = dropdown_width(items);
                int dc = dropdown_count(items);

                /* Calculate dropdown x origin (same as menu_render_dropdown) */
                int dx = 1;
                for (int i = 0; i < ms->top_item; i++)
                    dx += (int)strlen(g_top_items[i].label) + 2;
                int rows, cols;
                display_get_size(&rows, &cols);
                if (dx + dw > cols) dx = cols - dw;
                if (dx < 0) dx = 0;

                int rel_y = ev->my - 1;  /* dropdown starts at screen row 1 */
                int rel_x = ev->mx - dx;

                if (rel_y >= 0 && rel_y < dc && rel_x >= 0 && rel_x < dw) {
                    const SubItem *si = &items[rel_y];
                    if (si->label && si->cmd_id != 0) {
                        /* Find selectable index for this row */
                        int sel_idx = 0;
                        for (int i = 0; i < rel_y; i++)
                            if (items[i].label) sel_idx++;

                        int cmd = si->cmd_id;
                        ms->active = ms->dropdown_open = false;
                        ms->top_item = ms->sub_item = -1;
                        ed->needs_redraw = true;
                        if (execute_command(cmd, ed)) g_should_exit = true;
                        return true;
                    }
                } else {
                    /* Click outside dropdown → close */
                    ms->active = ms->dropdown_open = false;
                    ms->top_item = ms->sub_item = -1;
                    ed->needs_redraw = true;
                    return false;
                }
            } else {
                /* Menu bar active but no dropdown, click elsewhere → close */
                ms->active = false;
                ms->top_item = -1;
                ed->needs_redraw = true;
            }
        }
        return ms->active;
    }

    if (ev->key == KEY_ESCAPE) {
        ms->active = ms->dropdown_open = false;
        ms->top_item = ms->sub_item = -1;
        ed->needs_redraw = true;
        return true;
    }
    if (ev->key == KEY_LEFT) {
        ms->top_item = (ms->top_item - 1 + TOP_ITEM_COUNT) % TOP_ITEM_COUNT;
        ms->sub_item = 0;
        ed->needs_redraw = true;
        return true;
    }
    if (ev->key == KEY_RIGHT) {
        ms->top_item = (ms->top_item + 1) % TOP_ITEM_COUNT;
        ms->sub_item = 0;
        ed->needs_redraw = true;
        return true;
    }

    if (ms->dropdown_open) {
        const SubItem *items = g_top_items[ms->top_item].items;
        int selectable = 0;
        for (int i = 0; items[i].cmd_id != -1; i++)
            if (items[i].label) selectable++;

        if (ev->key == KEY_DOWN) {
            ms->sub_item = (ms->sub_item + 1) % selectable;
            ed->needs_redraw = true;
            return true;
        }
        if (ev->key == KEY_UP) {
            ms->sub_item = (ms->sub_item - 1 + selectable) % selectable;
            ed->needs_redraw = true;
            return true;
        }
        if (ev->key == KEY_ENTER) {
            int idx = 0;
            for (int i = 0; items[i].cmd_id != -1; i++) {
                if (!items[i].label) continue;
                if (idx == ms->sub_item) {
                    int cmd = items[i].cmd_id;
                    ms->active = ms->dropdown_open = false;
                    ms->top_item = ms->sub_item = -1;
                    ed->needs_redraw = true;
                    if (execute_command(cmd, ed)) g_should_exit = true;
                    return true;
                }
                idx++;
            }
        }
    } else {
        if (ev->key == KEY_DOWN || ev->key == KEY_ENTER) {
            ms->dropdown_open = true;
            ms->sub_item = 0;
            ed->needs_redraw = true;
            return true;
        }
    }

    return true;
}

/* ================================================================
 * Status bar
 * ================================================================ */

void render_status_bar(const Editor *ed) {
    int rows, cols;
    display_get_size(&rows, &cols);

    int sy = rows - 1;
    display_fill(sy, 0, cols - 1, ATTR_STATUS);

    char buf[128];
    snprintf(buf, sizeof(buf), " %s%s  Ln:%d Col:%d / %d lines  %s  %s",
             ed->doc->modified ? "*" : " ",
             editor_get_filename(ed),
             ed->cursor_row + 1,
             ed->cursor_col + 1,
             document_line_count(ed->doc),
             ed->insert_mode ? "INS" : "OVR",
             ed->vp.mode == WRAP_CHAR ? "WRAP" : "NWRP");

    /* 右侧：编码 + 语言名（若有语法定义），右对齐写入 */
    const char *enc_str = (ed->doc->encoding == DOC_ENC_GBK) ? "GBK" : "UTF-8";
    char right_buf[64];
    if (ed->syn_ctx.def != NULL) {
        snprintf(right_buf, sizeof(right_buf), "  %s  %s",
                 ed->syn_ctx.def->name, enc_str);
    } else {
        snprintf(right_buf, sizeof(right_buf), "  %s", enc_str);
    }
    int right_len = (int)strlen(right_buf);
    int right_col = cols - right_len;
    int left_len = (int)strlen(buf);
    if (right_col >= left_len) {
        display_put_str_n(sy, right_col, right_buf, right_len, ATTR_STATUS);
    }

    display_put_str_n(sy, 0, buf, right_col >= left_len ? left_len : cols, ATTR_STATUS);
}
