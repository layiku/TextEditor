/*
 * display.h — 显示抽象层接口
 * 管理双缓冲屏幕，提供平台无关的单元格写入接口；
 * display_flush() 将后备缓冲的变化差量输出到屏幕（最小化 I/O）。
 *
 * 不包含：布局计算、坐标换算、编辑区绘制（由 viewport.c 负责）
 */
#pragma once

#include <stdbool.h>
#include "types.h"

/* ================================================================
 * 生命周期
 * ================================================================ */
/* 初始化：获取终端尺寸，分配缓冲区，调用 plat_init() */
void display_init(void);

/* 退出：释放缓冲区，调用 plat_exit()，还原终端 */
void display_exit(void);

/* ================================================================
 * 尺寸查询
 * ================================================================ */
void display_get_size(int *rows, int *cols);

/* ================================================================
 * 写入后备缓冲（不立即输出，display_flush 时统一提交）
 * ================================================================ */
/* 在 (y, x) 写入单个字符单元格（cp 为 Unicode BMP 码点）。
 * 若 cp 为全宽字符（CJK 等），自动在 (y, x+1) 写入续格。 */
void display_put_cell(int y, int x, uint32_t cp, uint8_t attr);

/* 在 (y, x) 从 left 到 right（含两端）填充同一属性的空格 */
void display_fill(int y, int left, int right, uint8_t attr);

/* 在 (y, x) 写入字符串 s（不自动换行，超过屏幕宽度截断） */
void display_put_str(int y, int x, const char *s, uint8_t attr);

/* 在 (y, x) 写入最多 max_len 个字符（用于有最大宽度限制的文本） */
void display_put_str_n(int y, int x, const char *s, int max_len, uint8_t attr);

/* ================================================================
 * 光标控制（写入后备缓冲，flush 时生效）
 * ================================================================ */
void display_set_cursor(int y, int x);     /* 设置硬件光标位置 */
void display_show_cursor(bool show);       /* 显示/隐藏光标 */

/* ================================================================
 * 刷新：将后备缓冲与前缓冲比较，输出变化的单元格
 * 调用后前后缓冲同步，光标移动到设定位置
 * ================================================================ */
void display_flush(void);

/* ================================================================
 * 全屏操作
 * ================================================================ */
/* 用 attr 属性的空格填满整个后备缓冲（不立即输出，需 flush） */
void display_clear(uint8_t attr);

/* 标记整个屏幕为脏（下次 flush 时强制全量重绘，用于模式切换/resize） */
void display_invalidate(void);

/* 终端尺寸改变后调用（重新获取尺寸，重新分配缓冲）
 * 需要之后调用 display_invalidate + display_flush 重绘 */
void display_resize(void);
