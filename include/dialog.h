/*
 * dialog.h — Modal dialog box interface
 * All dialogs are blocking: they capture input until the user confirms or cancels.
 * Callers must call display_invalidate() is handled internally.
 */
#pragma once

#include <stdbool.h>

/* Text-input dialog. title/prompt are ASCII strings.
 * buf holds initial text and receives the result.
 * Returns true = confirmed (Enter), false = cancelled (Esc). */
bool dialog_input(const char *title, const char *prompt,
                  char *buf, int buf_size);

/* Yes/No confirmation dialog.
 * Returns true = Yes, false = No/Esc. */
bool dialog_confirm(const char *title, const char *msg);

/* Unsaved-changes prompt shown before New/Open/Exit.
 * Returns 0 = Save, 1 = Don't Save, 2 = Cancel. */
int dialog_save_prompt(const char *filename);

/* Single-button error message box. Blocks until any key. */
void dialog_error(const char *msg);

/* Find dialog (Ctrl+F). Fills find_buf; options is reserved for future flags.
 * Returns true when user pressed Enter with a non-empty search string. */
bool dialog_find(char *find_buf, int find_size, int *options);

/* Find-and-replace dialog (Ctrl+H).
 * Returns true when user confirmed both fields. */
bool dialog_replace(char *find_buf,    int find_size,
                    char *replace_buf, int replace_size,
                    int *options);

/* Keyboard-shortcuts help screen (F1). Blocks until any key. */
void dialog_help(void);

/* Right-click context menu. Appears at screen position (y, x).
 * Returns: 0=Cut, 1=Copy, 2=Paste, 3=Select All, 4=Find Selected, -1=cancelled. */
int dialog_context_menu(int y, int x);

/* ================================================================
 * 文件/目录选择对话框
 * ================================================================ */

/* 打开文件选择器：浏览目录并选择一个文件。
 * initial_dir — 初始目录（NULL 或空串 = 当前工作目录）
 * out_path    — 选中文件的完整路径（调用方分配缓冲）
 * out_size    — out_path 缓冲区长度
 * 返回 true = 用户选中文件，false = 取消。 */
bool dialog_file_pick_open(const char *title,
                           const char *initial_dir,
                           char *out_path, int out_size);

/* 保存文件选择器：浏览目录并输入/选择文件名。
 * initial_dir  — 初始目录（NULL 或空串 = 当前工作目录）
 * initial_name — 初始文件名（显示在底部输入框，NULL = 空）
 * out_path     — 输出完整路径
 * out_size     — out_path 缓冲区长度
 * 返回 true = 用户确认，false = 取消。 */
bool dialog_file_pick_save(const char *title,
                           const char *initial_dir,
                           const char *initial_name,
                           char *out_path, int out_size);

/* ================================================================
 * 列表选择对话框（Phase 7：手动切换语法高亮）
 * ================================================================ */

/* 显示可滚动的列表选择框，居中弹出。
 * title  — 对话框标题文字
 * items  — 字符串指针数组，共 count 项
 * count  — 选项数量；为 0 时立即返回 -1
 * 返回选中项的索引（0-based），按 Esc 或点击框外返回 -1。
 * 键盘：↑/↓ 移动，Enter 确认，Esc 取消。
 * 鼠标：左键单击列表行直接选中并返回。 */
int dialog_list_select(const char *title,
                        const char **items, int count);
