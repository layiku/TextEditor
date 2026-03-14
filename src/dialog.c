/*
 * dialog.c — Modal dialog box implementations
 * All dialogs are blocking: they render a popup, capture input, then return.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dialog.h"
#include "display.h"
#include "input.h"
#include "types.h"
#include "util.h"

#ifdef _WIN32
#  include <windows.h>
#  include <direct.h>   /* _getcwd */
#  define getcwd _getcwd
#else
#  include <unistd.h>
#endif

/* ================================================================
 * Internal helpers
 * ================================================================ */

/* Draws a w×h dialog box at (y,x) with an optional centered title. */
static void draw_dialog_box(int y, int x, int w, int h, const char *title) {
    for (int r = y; r < y + h; r++)
        display_fill(r, x, x + w - 1, ATTR_DIALOG_BG);

    display_fill(y,         x, x + w - 1, ATTR_TITLE);
    display_fill(y + h - 1, x, x + w - 1, ATTR_TITLE);
    for (int r = y + 1; r < y + h - 1; r++) {
        display_put_cell(r, x,         '|', ATTR_DIALOG_BG);
        display_put_cell(r, x + w - 1, '|', ATTR_DIALOG_BG);
    }

    if (title) {
        char buf[64];
        snprintf(buf, sizeof(buf), " %s ", title);
        int tx = x + (w - (int)strlen(buf)) / 2;
        display_put_str(y, tx, buf, ATTR_TITLE);
    }
}

/* Single-line text input widget. Returns true = Enter, false = Esc.
 * The existing buf content is shown as the initial value. */
static bool input_line(int y, int x, int w, char *buf, int buf_size) {
    int len    = (int)strlen(buf);
    int cursor = len;

    while (1) {
        display_fill(y, x, x + w - 1, ATTR_NORMAL);
        int draw_start = (cursor >= w) ? cursor - w + 1 : 0;
        for (int i = 0; i < w && draw_start + i < len; i++)
            display_put_cell(y, x + i, buf[draw_start + i], ATTR_NORMAL);
        display_set_cursor(y, x + (cursor - draw_start));
        display_show_cursor(true);
        display_flush();

        InputEvent ev;
        input_wait_event(&ev, -1);
        if (ev.type != EVT_KEY) continue;

        if (ev.key == KEY_ENTER)  return true;
        if (ev.key == KEY_ESCAPE) return false;

        if (ev.key == KEY_BACKSPACE && cursor > 0) {
            memmove(buf + cursor - 1, buf + cursor, (size_t)(len - cursor + 1));
            cursor--; len--;
        } else if (ev.key == KEY_DELETE && cursor < len) {
            memmove(buf + cursor, buf + cursor + 1, (size_t)(len - cursor));
            buf[--len] = '\0';
        } else if (ev.key == KEY_LEFT  && cursor > 0)   { cursor--; }
        else if (ev.key == KEY_RIGHT && cursor < len)    { cursor++; }
        else if (ev.key == KEY_HOME)                     { cursor = 0; }
        else if (ev.key == KEY_END)                      { cursor = len; }
        else if (ev.key >= 32 && ev.key < 256 && ev.mod == 0) {
            if (len < buf_size - 1) {
                memmove(buf + cursor + 1, buf + cursor, (size_t)(len - cursor + 1));
                buf[cursor++] = (char)ev.key;
                len++;
            }
        }
    }
}

/* ================================================================
 * Public dialog functions
 * ================================================================ */

bool dialog_input(const char *title, const char *prompt,
                  char *buf, int buf_size) {
    int rows, cols;
    display_get_size(&rows, &cols);

    const int w = 50, h = 5;
    int dy = rows / 2 - h / 2;
    int dx = cols / 2 - w / 2;

    draw_dialog_box(dy, dx, w, h, title);
    display_put_str(dy + 1, dx + 2, prompt, ATTR_DIALOG_BG);
    display_flush();

    bool ok = input_line(dy + 2, dx + 2, w - 4, buf, buf_size);
    display_invalidate();
    return ok;
}

bool dialog_confirm(const char *title, const char *msg) {
    int rows, cols;
    display_get_size(&rows, &cols);

    int w = (int)strlen(msg) + 8;
    if (w < 30) w = 30;
    const int h = 5;
    int dy = rows / 2 - h / 2;
    int dx = cols / 2 - w / 2;

    draw_dialog_box(dy, dx, w, h, title);
    display_put_str(dy + 1, dx + 2, msg, ATTR_DIALOG_BG);
    display_put_str(dy + 3, dx + 2, "[ Yes(Y) ]   [ No(N) ]", ATTR_DIALOG_BG);
    display_flush();

    while (1) {
        InputEvent ev;
        input_wait_event(&ev, -1);
        if (ev.type != EVT_KEY) continue;
        if (ev.key == 'Y' || ev.key == 'y' || ev.key == KEY_ENTER)
            { display_invalidate(); return true; }
        if (ev.key == 'N' || ev.key == 'n' || ev.key == KEY_ESCAPE)
            { display_invalidate(); return false; }
    }
}

int dialog_save_prompt(const char *filename) {
    int rows, cols;
    display_get_size(&rows, &cols);

    char msg[80];
    snprintf(msg, sizeof(msg), "'%s' modified. Save?", filename);

    int w = (int)strlen(msg) + 8;
    if (w < 44) w = 44;
    const int h = 5;
    int dy = rows / 2 - h / 2;
    int dx = cols / 2 - w / 2;

    draw_dialog_box(dy, dx, w, h, "Save Changes");
    display_put_str(dy + 1, dx + 2, msg, ATTR_DIALOG_BG);
    display_put_str(dy + 3, dx + 2, "[Save(S)]  [Don't Save(D)]  [Cancel(C)]",
                    ATTR_DIALOG_BG);
    display_flush();

    while (1) {
        InputEvent ev;
        input_wait_event(&ev, -1);
        if (ev.type != EVT_KEY) continue;
        int k = ev.key;
        if (k == 'S' || k == 's') { display_invalidate(); return 0; }
        if (k == 'D' || k == 'd') { display_invalidate(); return 1; }
        if (k == 'C' || k == 'c' || k == KEY_ESCAPE) { display_invalidate(); return 2; }
    }
}

void dialog_error(const char *msg) {
    int rows, cols;
    display_get_size(&rows, &cols);

    int w = (int)strlen(msg) + 8;
    if (w < 30) w = 30;
    const int h = 4;
    int dy = rows / 2 - h / 2;
    int dx = cols / 2 - w / 2;

    draw_dialog_box(dy, dx, w, h, "Error");
    display_put_str(dy + 1, dx + 2, msg, ATTR_ERROR);
    display_put_str(dy + 2, dx + 2, "Press any key...", ATTR_DIALOG_BG);
    display_flush();

    InputEvent ev;
    input_wait_event(&ev, -1);
    display_invalidate();
}

bool dialog_find(char *find_buf, int find_size, int *options) {
    int rows, cols;
    display_get_size(&rows, &cols);

    const int w = 50, h = 5;
    int dy = rows / 2 - h / 2;
    int dx = cols / 2 - w / 2;

    draw_dialog_box(dy, dx, w, h, "Find");
    display_put_str(dy + 1, dx + 2, "Search:", ATTR_DIALOG_BG);
    display_flush();

    bool ok = input_line(dy + 2, dx + 2, w - 4, find_buf, find_size);
    (void)options;
    display_invalidate();
    return ok && find_buf[0];
}

bool dialog_replace(char *find_buf,    int find_size,
                    char *replace_buf, int replace_size,
                    int *options) {
    int rows, cols;
    display_get_size(&rows, &cols);

    const int w = 50, h = 7;
    int dy = rows / 2 - h / 2;
    int dx = cols / 2 - w / 2;

    draw_dialog_box(dy, dx, w, h, "Replace");
    display_put_str(dy + 1, dx + 2, "Search: ", ATTR_DIALOG_BG);
    display_flush();

    if (!input_line(dy + 2, dx + 2, w - 4, find_buf, find_size)) {
        display_invalidate();
        return false;
    }
    display_put_str(dy + 3, dx + 2, "Replace:", ATTR_DIALOG_BG);
    display_flush();

    bool ok = input_line(dy + 4, dx + 2, w - 4, replace_buf, replace_size);
    (void)options;
    display_invalidate();
    return ok;
}

void dialog_help(void) {
    int rows, cols;
    display_get_size(&rows, &cols);

    static const char *lines[] = {
        "  Ctrl+N        New file",
        "  Ctrl+O        Open file",
        "  Ctrl+S / F2   Save",
        "  Ctrl+Shift+S  Save As",
        "  Ctrl+Z        Undo",
        "  Ctrl+Y        Redo",
        "  Ctrl+X/C/V    Cut / Copy / Paste",
        "  Ctrl+A        Select All",
        "  Ctrl+F        Find",
        "  F3            Find Next",
        "  Shift+F3      Find Prev",
        "  Ctrl+H        Replace",
        "  Ctrl+W        Toggle Wrap / HScroll",
        "  Ctrl+</>      Horizontal scroll",
        "  Insert        Toggle Insert/Overwrite",
        "  Alt+X         Exit",
        NULL
    };

    int n = 0;
    while (lines[n]) n++;

    const int w = 46;
    int h  = n + 4;
    int dy = (rows - h) / 2;
    int dx = (cols - w) / 2;
    if (dy < 0) dy = 0;
    if (dx < 0) dx = 0;

    draw_dialog_box(dy, dx, w, h, "Keyboard Shortcuts (F1)");
    for (int i = 0; i < n; i++)
        display_put_str_n(dy + 1 + i, dx + 1, lines[i], w - 2, ATTR_DIALOG_BG);
    display_put_str(dy + h - 2, dx + 2, "Press any key to close", ATTR_DIALOG_BG);
    display_flush();

    InputEvent ev;
    input_wait_event(&ev, -1);
    display_invalidate();
}

int dialog_list_select(const char *title,
                        const char **items, int count) {
    if (!items || count <= 0) return -1;

    int rows, cols;
    display_get_size(&rows, &cols);

    /* ---- 计算对话框尺寸 ---- */
    /* 宽度 = max(title长度+4, 最长选项+4, 34)，不超过 cols-4 */
    int w = 34;
    if (title) {
        int tl = (int)strlen(title) + 4;
        if (tl > w) w = tl;
    }
    for (int i = 0; i < count; i++) {
        if (!items[i]) continue;
        int il = (int)strlen(items[i]) + 4;
        if (il > w) w = il;
    }
    if (w > cols - 4) w = cols - 4;
    if (w < 20) w = 20;

    /* 提示行固定1行，边框上下各1行，可见列表行 */
    int max_visible = rows - 6;
    if (max_visible < 3)  max_visible = 3;
    if (max_visible > count) max_visible = count;
    int h = max_visible + 3;  /* 上边框 + 列表行 + 提示行 + 下边框 */

    /* 居中位置 */
    int bx = (cols - w) / 2;
    int by = (rows - h) / 2;
    if (bx < 0) bx = 0;
    if (by < 0) by = 0;

    int list_top = by + 1;           /* 列表第一个可见行的屏幕 y */
    int hint_row = by + h - 2;       /* 提示文字行 */
    int inner_w  = w - 2;            /* 内部可用宽度 */

    int cur        = 0;
    int scroll_top = 0;

    while (1) {
        /* ---- 渲染框体 ---- */
        draw_dialog_box(by, bx, w, h, title);

        /* ---- 渲染列表行 ---- */
        for (int vi = 0; vi < max_visible; vi++) {
            int idx = scroll_top + vi;
            if (idx >= count) break;

            uint8_t attr = (idx == cur) ? ATTR_MENU_SEL : ATTR_DIALOG_BG;
            int sy = list_top + vi;
            display_fill(sy, bx + 1, bx + w - 2, attr);

            /* 显示选项文字，前缀用 ">" 或 " " 标记当前项 */
            char row_buf[256];
            snprintf(row_buf, sizeof(row_buf), "%c %-*.*s",
                     (idx == cur) ? '>' : ' ',
                     inner_w - 2, inner_w - 2,
                     items[idx] ? items[idx] : "");
            display_put_str_n(sy, bx + 1, row_buf, inner_w, attr);
        }

        /* ---- 渲染提示文字 ---- */
        {
            uint8_t ha = ATTR_DIALOG_BG;
            display_fill(hint_row, bx + 1, bx + w - 2, ha);
            const char *hint = " ↑/↓ 选择  Enter 确认  Esc 取消";
            display_put_str_n(hint_row, bx + 1, hint, inner_w, ha);
        }

        display_flush();

        /* ---- 事件处理 ---- */
        InputEvent ev;
        input_wait_event(&ev, -1);

        if (ev.type == EVT_KEY) {
            if (ev.key == KEY_ESCAPE) {
                display_invalidate();
                return -1;
            } else if (ev.key == KEY_UP) {
                if (cur > 0) {
                    cur--;
                    if (cur < scroll_top) scroll_top = cur;
                }
            } else if (ev.key == KEY_DOWN) {
                if (cur < count - 1) {
                    cur++;
                    if (cur >= scroll_top + max_visible)
                        scroll_top = cur - max_visible + 1;
                }
            } else if (ev.key == KEY_ENTER) {
                display_invalidate();
                return cur;
            }
        } else if (ev.type == EVT_MOUSE_DOWN) {
            int rel_y = ev.my - list_top;
            int rel_x = ev.mx - bx;
            if (rel_x >= 0 && rel_x < w &&
                rel_y >= 0 && rel_y < max_visible) {
                /* 左键点击列表行：直接确认 */
                int clicked = scroll_top + rel_y;
                if (clicked < count) {
                    display_invalidate();
                    return clicked;
                }
            } else {
                /* 点击框外：取消 */
                display_invalidate();
                return -1;
            }
        }
    }
}

int dialog_context_menu(int y, int x) {
    static const char *labels[] = {
        " Cut        Ctrl+X ",
        " Copy       Ctrl+C ",
        " Paste      Ctrl+V ",
        "-------------------",
        " Select All Ctrl+A ",
        " Find Selected     ",
        NULL
    };
    /* Map row index → return value; -1 = separator (non-selectable) */
    static const int cmd_ids[] = { 0, 1, 2, -1, 3, 4 };

    int n = 0;
    while (labels[n]) n++;

    const int w  = 21;
    int rows, cols;
    display_get_size(&rows, &cols);

    /* Clamp popup so it stays entirely on screen */
    int px = x, py = y + 1;
    if (px + w > cols)  px = cols - w;
    if (px < 0)         px = 0;
    if (py + n > rows)  py = y - n;
    if (py < 0)         py = 0;

    /* Start highlight on first selectable item */
    int cur = 0;
    while (cur < n && cmd_ids[cur] == -1) cur++;

    while (1) {
        for (int i = 0; i < n; i++) {
            uint8_t attr = (i == cur && cmd_ids[i] != -1)
                           ? ATTR_MENU_SEL : ATTR_DIALOG_BG;
            /* Fill entire row width first so no background bleeds through */
            display_fill(py + i, px, px + w - 1, attr);
            display_put_str_n(py + i, px, labels[i], w, attr);
        }
        display_flush();

        InputEvent ev;
        input_wait_event(&ev, -1);

        if (ev.type == EVT_KEY) {
            if (ev.key == KEY_ESCAPE) {
                display_invalidate(); return -1;
            } else if (ev.key == KEY_UP) {
                do { cur--; if (cur < 0) cur = n - 1; } while (cmd_ids[cur] == -1);
            } else if (ev.key == KEY_DOWN) {
                do { cur++; if (cur >= n) cur = 0; } while (cmd_ids[cur] == -1);
            } else if (ev.key == KEY_ENTER) {
                display_invalidate(); return cmd_ids[cur];
            }
        } else if (ev.type == EVT_MOUSE_DOWN) {
            int rel_y = ev.my - py;
            int rel_x = ev.mx - px;
            if (rel_y >= 0 && rel_y < n && rel_x >= 0 && rel_x < w) {
                if (cmd_ids[rel_y] != -1) {
                    display_invalidate(); return cmd_ids[rel_y];
                }
                /* Click on separator: stay open */
            } else {
                display_invalidate(); return -1;
            }
        }
    }
}

/* ================================================================
 * 文件选择器内部共享实现
 * ================================================================ */

#define FILE_PICK_MAX 512   /* 单次目录最多显示条目数 */

/* 在对话框顶部画当前路径（截断显示） */
static void draw_current_dir(int dy, int dx, int w, const char *cur_dir) {
    char buf[300];
    int max_w = w - 4;
    int dlen  = (int)strlen(cur_dir);
    if (dlen > max_w) {
        /* 显示末尾部分，前面加 "..." */
        snprintf(buf, sizeof(buf), "...%s", cur_dir + dlen - (max_w - 3));
    } else {
        snprintf(buf, sizeof(buf), "%s", cur_dir);
    }
    display_fill(dy + 1, dx + 1, dx + w - 2, ATTR_DIALOG_BG);
    display_put_str_n(dy + 1, dx + 2, buf, w - 4, ATTR_DIALOG_BG);
}

/* 共享文件选择逻辑。
 * for_save=false : 打开，只能选文件（目录可进入）
 * for_save=true  : 保存，目录可进入，底部有文件名输入框 */
static bool file_picker_impl(const char *title,
                              const char *initial_dir,
                              const char *initial_name,
                              bool        for_save,
                              char       *out_path,
                              int         out_size) {
    int rows, cols;
    display_get_size(&rows, &cols);

    /* 当前目录 */
    char cur_dir[512];
    if (initial_dir && initial_dir[0]) {
        strncpy(cur_dir, initial_dir, sizeof(cur_dir) - 1);
        cur_dir[sizeof(cur_dir) - 1] = '\0';
    } else {
        if (!getcwd(cur_dir, sizeof(cur_dir)))
            strncpy(cur_dir, ".", sizeof(cur_dir) - 1);
    }

    /* 文件名输入缓冲（仅 for_save 使用） */
    char name_buf[256] = "";
    if (initial_name && initial_name[0])
        strncpy(name_buf, initial_name, sizeof(name_buf) - 1);

    /* 条目缓冲 */
    DirEntry *entries = (DirEntry *)malloc(FILE_PICK_MAX * sizeof(DirEntry));
    if (!entries) return false;

    /* 对话框尺寸 */
    int w = cols - 6;
    if (w < 40) w = 40;
    if (w > 90) w = 90;

    /* 行高：for_save 多一行输入框 */
    int extra = for_save ? 3 : 0;   /* 分隔线 + 标签 + 输入框 */
    int list_h = rows - 10 - extra;
    if (list_h < 4) list_h = 4;
    int h = list_h + 5 + extra;     /* 上框 + 路径 + 列表 + 提示 + 下框 */

    int bx = (cols - w) / 2;
    int by = (rows - h) / 2;
    if (bx < 0) bx = 0;
    if (by < 0) by = 0;

    int list_top = by + 2;          /* 列表第一行屏幕 y */
    int hint_row = by + h - 2;      /* 提示行 */
    int inner_w  = w - 2;

    /* for_save 的输入框行 */
    int sep_row   = hint_row - extra;
    int fname_row = hint_row - extra + 2;

    int cur_sel    = 0;
    int scroll_top = 0;
    int entry_count = 0;

    /* 双击检测 */
    long last_click_ms = 0;
    int  last_click_idx = -1;

    /* 加载目录 */
    entry_count = dir_read_entries(cur_dir, entries, FILE_PICK_MAX);

    while (1) {
        /* ---- 渲染框体 ---- */
        draw_dialog_box(by, bx, w, h, title);
        draw_current_dir(by, bx, w, cur_dir);

        /* ---- 列表 ---- */
        for (int vi = 0; vi < list_h; vi++) {
            int idx = scroll_top + vi;
            int sy  = list_top + vi;
            display_fill(sy, bx + 1, bx + w - 2, ATTR_DIALOG_BG);
            if (idx >= entry_count) continue;

            uint8_t attr = (idx == cur_sel) ? ATTR_MENU_SEL : ATTR_DIALOG_BG;
            display_fill(sy, bx + 1, bx + w - 2, attr);

            char row_buf[290];
            if (entries[idx].is_dir) {
                snprintf(row_buf, sizeof(row_buf), "%c [%s]",
                         (idx == cur_sel) ? '>' : ' ', entries[idx].name);
            } else {
                snprintf(row_buf, sizeof(row_buf), "%c  %s",
                         (idx == cur_sel) ? '>' : ' ', entries[idx].name);
            }
            display_put_str_n(sy, bx + 1, row_buf, inner_w, attr);
        }

        /* ---- for_save：分隔线 + 文件名输入框 ---- */
        if (for_save) {
            display_fill(sep_row, bx + 1, bx + w - 2, ATTR_DIALOG_BG);
            display_put_str_n(sep_row, bx + 1,
                              "----------------------------------------",
                              inner_w, ATTR_DIALOG_BG);
            display_fill(sep_row + 1, bx + 1, bx + w - 2, ATTR_DIALOG_BG);
            display_put_str_n(sep_row + 1, bx + 2, "File name:", inner_w - 1, ATTR_DIALOG_BG);
            display_fill(fname_row, bx + 1, bx + w - 2, ATTR_NORMAL);
            display_put_str_n(fname_row, bx + 2, name_buf, inner_w - 2, ATTR_NORMAL);
        }

        /* ---- 提示 ---- */
        display_fill(hint_row, bx + 1, bx + w - 2, ATTR_DIALOG_BG);
        if (for_save)
            display_put_str_n(hint_row, bx + 1,
                              " Enter:Open/Name  Tab:FileName  /:GotoPath  Esc:Cancel",
                              inner_w, ATTR_DIALOG_BG);
        else
            display_put_str_n(hint_row, bx + 1,
                              " Enter:Open/Select  /:GotoPath  Esc:Cancel",
                              inner_w, ATTR_DIALOG_BG);

        display_show_cursor(false);
        display_flush();

        /* ---- 事件 ---- */
        InputEvent ev;
        input_wait_event(&ev, -1);

        if (ev.type == EVT_KEY) {
            int k = ev.key;

            if (k == KEY_ESCAPE) {
                free(entries);
                display_invalidate();
                return false;
            }

            if (k == KEY_UP && cur_sel > 0) {
                cur_sel--;
                if (cur_sel < scroll_top) scroll_top = cur_sel;
            } else if (k == KEY_DOWN && cur_sel < entry_count - 1) {
                cur_sel++;
                if (cur_sel >= scroll_top + list_h)
                    scroll_top = cur_sel - list_h + 1;
            } else if (k == KEY_ENTER) {
                if (entry_count == 0) continue;

                if (entries[cur_sel].is_dir) {
                    /* 进入目录 */
                    char new_dir[512];
                    if (strcmp(entries[cur_sel].name, "..") == 0) {
                        path_get_dir(cur_dir, new_dir, sizeof(new_dir));
                    } else {
                        path_join(cur_dir, entries[cur_sel].name,
                                  new_dir, sizeof(new_dir));
                    }
                    strncpy(cur_dir, new_dir, sizeof(cur_dir) - 1);
                    cur_dir[sizeof(cur_dir) - 1] = '\0';
                    cur_sel    = 0;
                    scroll_top = 0;
                    entry_count = dir_read_entries(cur_dir, entries, FILE_PICK_MAX);
                } else {
                    /* 选中文件 */
                    if (for_save) {
                        /* 将文件名填入输入框，然后要用 Tab/Enter 二次确认 */
                        strncpy(name_buf, entries[cur_sel].name, sizeof(name_buf) - 1);
                        /* 切换焦点到输入框：直接弹出输入对话框 */
                        char tmp[256];
                        strncpy(tmp, name_buf, sizeof(tmp) - 1);
                        /* 在当前对话框内做内联编辑（复用 input_line 静态函数不可访问，
                           此处用 dialog_input 作简化替代） */
                        if (dialog_input("Save As", "File name:", tmp, sizeof(tmp)) && tmp[0]) {
                            path_join(cur_dir, tmp, out_path, out_size);
                            free(entries);
                            display_invalidate();
                            return true;
                        }
                        /* 用户取消了输入，继续浏览 */
                    } else {
                        path_join(cur_dir, entries[cur_sel].name,
                                  out_path, out_size);
                        free(entries);
                        display_invalidate();
                        return true;
                    }
                }
            } else if (k == KEY_TAB && for_save) {
                /* Tab 键：聚焦到文件名输入框 */
                char tmp[256];
                strncpy(tmp, name_buf, sizeof(tmp) - 1);
                if (dialog_input("Save As", "File name:", tmp, sizeof(tmp)) && tmp[0]) {
                    path_join(cur_dir, tmp, out_path, out_size);
                    free(entries);
                    display_invalidate();
                    return true;
                }
            } else if (k == '/' || k == '\\' || k == '~' ||
                       (ev.mod == MOD_CTRL && k == 'G')) {
                /* 直接输入路径：预填当前目录，用户可修改后跳转 */
                char path_buf[512];
                strncpy(path_buf, cur_dir, sizeof(path_buf) - 1);
                path_buf[sizeof(path_buf) - 1] = '\0';
                /* 若按下的是可打印路径分隔符，将其追加到末尾作为起始字符 */
                if ((k == '/' || k == '\\') && ev.mod == 0) {
                    int plen = (int)strlen(path_buf);
                    if (plen < (int)sizeof(path_buf) - 2) {
                        path_buf[plen]     = (char)k;
                        path_buf[plen + 1] = '\0';
                    }
                } else if (k == '~' && ev.mod == 0) {
                    /* ~ 跳转到用户主目录 */
                    get_user_config_dir(path_buf, sizeof(path_buf));
                }
                if (dialog_input("Go to Path", "Path:", path_buf, sizeof(path_buf))
                    && path_buf[0]) {
                    if (path_is_dir(path_buf)) {
                        /* 目标是目录：导航进入 */
                        strncpy(cur_dir, path_buf, sizeof(cur_dir) - 1);
                        cur_dir[sizeof(cur_dir) - 1] = '\0';
                        cur_sel     = 0;
                        scroll_top  = 0;
                        entry_count = dir_read_entries(cur_dir, entries, FILE_PICK_MAX);
                    } else if (!for_save && file_exists(path_buf)) {
                        /* 目标是文件（打开模式）：直接选中 */
                        strncpy(out_path, path_buf, (size_t)(out_size - 1));
                        out_path[out_size - 1] = '\0';
                        free(entries);
                        display_invalidate();
                        return true;
                    } else if (for_save) {
                        /* 保存模式：路径中分离目录与文件名 */
                        char dir_part[512];
                        path_get_dir(path_buf, dir_part, sizeof(dir_part));
                        if (path_is_dir(dir_part)) {
                            strncpy(out_path, path_buf, (size_t)(out_size - 1));
                            out_path[out_size - 1] = '\0';
                            free(entries);
                            display_invalidate();
                            return true;
                        }
                        /* 目录不存在：导航到最近有效目录 */
                        strncpy(cur_dir, dir_part, sizeof(cur_dir) - 1);
                        cur_dir[sizeof(cur_dir) - 1] = '\0';
                        cur_sel     = 0;
                        scroll_top  = 0;
                        entry_count = dir_read_entries(cur_dir, entries, FILE_PICK_MAX);
                    } else {
                        dialog_error("Path not found.");
                    }
                }
            }

        } else if (ev.type == EVT_MOUSE_DOWN) {
            int rel_y = ev.my - list_top;
            int rel_x = ev.mx - bx;

            if (rel_x >= 0 && rel_x < w &&
                rel_y >= 0 && rel_y < list_h) {
                int clicked = scroll_top + rel_y;
                if (clicked >= entry_count) continue;

                /* 单击：移动选中 */
                cur_sel = clicked;

                /* 双击检测：同一行 500ms 内连续两次按下 */
                bool is_dbl = false;
                {
                    long now = 0;
#ifdef _WIN32
                    now = (long)GetTickCount();
#endif
                    if (last_click_idx == clicked &&
                        (now - last_click_ms) < 500)
                        is_dbl = true;
                    last_click_ms  = now;
                    last_click_idx = clicked;
                }

                if (is_dbl) {
                    if (entries[clicked].is_dir) {
                        char new_dir[512];
                        if (strcmp(entries[clicked].name, "..") == 0) {
                            path_get_dir(cur_dir, new_dir, sizeof(new_dir));
                        } else {
                            path_join(cur_dir, entries[clicked].name,
                                      new_dir, sizeof(new_dir));
                        }
                        strncpy(cur_dir, new_dir, sizeof(cur_dir) - 1);
                        cur_dir[sizeof(cur_dir) - 1] = '\0';
                        cur_sel     = 0;
                        scroll_top  = 0;
                        last_click_idx = -1;
                        entry_count = dir_read_entries(cur_dir, entries, FILE_PICK_MAX);
                    } else {
                        if (for_save) {
                            char tmp[256];
                            strncpy(tmp, entries[clicked].name, sizeof(tmp) - 1);
                            if (dialog_input("Save As", "File name:", tmp, sizeof(tmp)) && tmp[0]) {
                                path_join(cur_dir, tmp, out_path, out_size);
                                free(entries);
                                display_invalidate();
                                return true;
                            }
                        } else {
                            path_join(cur_dir, entries[clicked].name,
                                      out_path, out_size);
                            free(entries);
                            display_invalidate();
                            return true;
                        }
                    }
                }

            } else if (for_save &&
                       ev.mx >= bx + 1 && ev.mx < bx + w - 1 &&
                       ev.my == fname_row) {
                /* 点击文件名输入框 */
                char tmp[256];
                strncpy(tmp, name_buf, sizeof(tmp) - 1);
                if (dialog_input("Save As", "File name:", tmp, sizeof(tmp)) && tmp[0]) {
                    path_join(cur_dir, tmp, out_path, out_size);
                    free(entries);
                    display_invalidate();
                    return true;
                }
            } else if (ev.mx >= bx && ev.mx < bx + w &&
                       ev.my == by + 1) {
                /* 点击路径栏：弹出路径输入框 */
                char path_buf[512];
                strncpy(path_buf, cur_dir, sizeof(path_buf) - 1);
                path_buf[sizeof(path_buf) - 1] = '\0';
                if (dialog_input("Go to Path", "Path:", path_buf, sizeof(path_buf))
                    && path_buf[0]) {
                    if (path_is_dir(path_buf)) {
                        strncpy(cur_dir, path_buf, sizeof(cur_dir) - 1);
                        cur_dir[sizeof(cur_dir) - 1] = '\0';
                        cur_sel     = 0;
                        scroll_top  = 0;
                        last_click_idx = -1;
                        entry_count = dir_read_entries(cur_dir, entries, FILE_PICK_MAX);
                    } else if (!for_save && file_exists(path_buf)) {
                        strncpy(out_path, path_buf, (size_t)(out_size - 1));
                        out_path[out_size - 1] = '\0';
                        free(entries);
                        display_invalidate();
                        return true;
                    } else if (for_save) {
                        char dir_part[512];
                        path_get_dir(path_buf, dir_part, sizeof(dir_part));
                        if (path_is_dir(dir_part)) {
                            strncpy(out_path, path_buf, (size_t)(out_size - 1));
                            out_path[out_size - 1] = '\0';
                            free(entries);
                            display_invalidate();
                            return true;
                        }
                    } else {
                        dialog_error("Path not found.");
                    }
                }
            } else {
                /* 点击框外：取消 */
                free(entries);
                display_invalidate();
                return false;
            }

        } else if (ev.type == EVT_SCROLL) {
            /* mscroll: 正数=向上，负数=向下 */
            if (ev.mscroll > 0 && scroll_top > 0) {
                scroll_top--;
                if (cur_sel >= scroll_top + list_h)
                    cur_sel = scroll_top + list_h - 1;
            } else if (ev.mscroll < 0 &&
                       scroll_top + list_h < entry_count) {
                scroll_top++;
                if (cur_sel < scroll_top) cur_sel = scroll_top;
            }
        }
    }
}

bool dialog_file_pick_open(const char *title,
                            const char *initial_dir,
                            char *out_path, int out_size) {
    return file_picker_impl(title, initial_dir, NULL, false, out_path, out_size);
}

bool dialog_file_pick_save(const char *title,
                            const char *initial_dir,
                            const char *initial_name,
                            char *out_path, int out_size) {
    return file_picker_impl(title, initial_dir, initial_name, true, out_path, out_size);
}
