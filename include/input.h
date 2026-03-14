/*
 * input.h — 输入处理接口
 * 对 plat_poll_event 的轻量封装，提供统一的事件读取接口
 */
#pragma once

#include "types.h"

/* 初始化（程序启动时调用；plat_init 已在 display_init 中调用，这里做应用层初始化） */
void input_init(void);

/* 退出清理 */
void input_exit(void);

/* 等待并读取一个事件，最多阻塞 timeout_ms 毫秒（-1 = 永久等待）
 * 有事件返回 1，无事件（超时）返回 0 */
int input_wait_event(InputEvent *ev, int timeout_ms);

/* 非阻塞轮询（等价于 input_wait_event(ev, 0)） */
int input_poll(InputEvent *ev);

/* 辅助宏：判断事件是否为指定 Ctrl 组合键（key 为字母，如 'C' 表示 Ctrl+C） */
#define IS_CTRL(ev, letter) \
    ((ev)->type == EVT_KEY && (ev)->key == (letter) && ((ev)->mod & MOD_CTRL))

/* 辅助宏：判断事件是否为指定 Alt 组合键 */
#define IS_ALT(ev, letter) \
    ((ev)->type == EVT_KEY && (ev)->key == (letter) && ((ev)->mod & MOD_ALT))

/* 辅助宏：判断事件是否为指定功能键（无修饰键） */
#define IS_KEY(ev, k) \
    ((ev)->type == EVT_KEY && (ev)->key == (k) && (ev)->mod == 0)

/* 辅助宏：判断事件是否为 Shift+功能键 */
#define IS_SHIFT_KEY(ev, k) \
    ((ev)->type == EVT_KEY && (ev)->key == (k) && ((ev)->mod == MOD_SHIFT))
