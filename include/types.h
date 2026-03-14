/*
 * types.h — 全局类型定义与常量
 * 所有模块共用的基础结构，不依赖任何模块头文件
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ================================================================
 * 屏幕单元：字符 + 属性
 *
 * ch    — Unicode BMP 码点（0 = 宽字符的右半"续格"，1..0xFFFF = 实际字符）
 * width — 0=续格（宽字符右半），1=半宽（ASCII/Latin），2=全宽（CJK 等）
 * attr  — 高 4 位=背景色，低 4 位=前景色
 *
 * 宽字符渲染规则：
 *   display_put_cell(y, x, cp, attr) 当 cp 为全宽字符时，
 *   同时将 Cell[x].width=2 和 Cell[x+1] 设为续格（ch=0, width=0, attr=attr）
 * ================================================================ */
typedef struct {
    uint16_t ch;    /* Unicode BMP 码点（0 = 宽字符续格） */
    uint8_t  width; /* 0=续格, 1=半宽, 2=全宽 */
    uint8_t  attr;  /* 颜色属性 */
} Cell;

/* ================================================================
 * 屏幕缓冲区：维护 front（已输出到屏幕）和 back（待写入）两帧
 * display_flush() 比较两帧，只输出变化的单元格，减少 I/O 和闪烁
 * ================================================================ */
typedef struct {
    int   rows, cols;  /* 终端尺寸（动态，可随窗口调整变化） */
    Cell *front;       /* 当前帧（反映屏幕真实内容） */
    Cell *back;        /* 后备帧（下一帧正在写入） */
} ScreenBuffer;

/* ================================================================
 * 换行模式
 * ================================================================ */
typedef enum {
    WRAP_CHAR = 0,   /* 字符折行：长行按编辑区宽强制折成多显示行 */
    WRAP_NONE = 1    /* 横滚模式：每逻辑行一屏幕行，超长部分水平滚动查看 */
} WrapMode;

/* ================================================================
 * 输入事件类型
 * ================================================================ */
typedef enum {
    EVT_NONE       = 0,
    EVT_KEY        = 1,   /* 键盘事件 */
    EVT_MOUSE_DOWN = 2,   /* 鼠标按下 */
    EVT_MOUSE_UP   = 3,   /* 鼠标释放 */
    EVT_MOUSE_MOVE = 4,   /* 鼠标移动 */
    EVT_RESIZE     = 5,   /* 终端尺寸变化 */
    EVT_SCROLL     = 6    /* 鼠标滚轮滚动 */
} InputEventType;

/* 修饰键位标志（可组合） */
#define MOD_SHIFT  0x01
#define MOD_CTRL   0x02
#define MOD_ALT    0x04

/* 虚拟键码（可打印字符直接用 ASCII；特殊键从 256 起，避免与 ASCII 冲突） */
#define KEY_NONE       0
#define KEY_UP         256
#define KEY_DOWN       257
#define KEY_LEFT       258
#define KEY_RIGHT      259
#define KEY_HOME       260
#define KEY_END        261
#define KEY_PGUP       262
#define KEY_PGDN       263
#define KEY_INSERT     264
#define KEY_DELETE     265
#define KEY_BACKSPACE  266
#define KEY_ENTER      267
#define KEY_ESCAPE     268
#define KEY_TAB        269
#define KEY_F1         270
#define KEY_F2         271
#define KEY_F3         272
#define KEY_F4         273
#define KEY_F5         274
#define KEY_F6         275
#define KEY_F7         276
#define KEY_F8         277
#define KEY_F9         278
#define KEY_F10        279
#define KEY_F11        280
#define KEY_F12        281

/* ================================================================
 * 输入事件结构
 * ================================================================ */
typedef struct {
    InputEventType type;
    int key;      /* 虚拟键码 或 可打印字符的 ASCII 值 */
    int mod;      /* 修饰键：MOD_SHIFT | MOD_CTRL | MOD_ALT 的组合 */
    int mx, my;   /* 鼠标屏幕坐标（仅 MOUSE_* 事件有效，0-based） */
    int mbutton;  /* 鼠标按键：0=左，1=右，2=中 */
    int mscroll;  /* 滚轮方向（EVT_SCROLL 有效）：负数=向下滚，正数=向上滚 */
} InputEvent;

/* ================================================================
 * 字符属性与颜色（VGA 16 色编码）
 * 低 4 位 = 前景色，高 4 位 = 背景色
 * ================================================================ */
#define COLOR_BLACK    0
#define COLOR_BLUE     1
#define COLOR_GREEN    2
#define COLOR_CYAN     3
#define COLOR_RED      4
#define COLOR_MAGENTA  5
#define COLOR_BROWN    6
#define COLOR_WHITE    7
#define COLOR_BRIGHT   8   /* 与前景色 OR，使其变亮（如 7|8=15=亮白） */

/* 构造属性宏：attr = MAKE_ATTR(前景色, 背景色) */
#define MAKE_ATTR(fg, bg) (uint8_t)(((bg) << 4) | ((fg) & 0x0F))

/* Predefined attributes — all expressed via MAKE_ATTR(fg, bg) for clarity */
#define ATTR_NORMAL      MAKE_ATTR(COLOR_WHITE,                  COLOR_BLACK)
#define ATTR_REVERSE     MAKE_ATTR(COLOR_BLACK,                  COLOR_WHITE)
#define ATTR_LINENO      MAKE_ATTR(COLOR_BRIGHT,                 COLOR_BLACK)
#define ATTR_MENUBAR     MAKE_ATTR(COLOR_BLACK,                  COLOR_WHITE)
#define ATTR_MENU_SEL    MAKE_ATTR(COLOR_WHITE,                  COLOR_BLUE)
#define ATTR_MENU_ACCEL  MAKE_ATTR(COLOR_WHITE | COLOR_BRIGHT,   COLOR_BLUE)
#define ATTR_STATUS      MAKE_ATTR(COLOR_BLACK,                  COLOR_WHITE)
#define ATTR_DIALOG_BG   MAKE_ATTR(COLOR_BLACK,                  COLOR_WHITE)
#define ATTR_DIALOG_BTN  MAKE_ATTR(COLOR_WHITE,                  COLOR_BLACK)
#define ATTR_DIALOG_SEL  MAKE_ATTR(COLOR_WHITE,                  COLOR_BLUE)
#define ATTR_ERROR       MAKE_ATTR(COLOR_RED | COLOR_BRIGHT,     COLOR_BLACK)
#define ATTR_TITLE       MAKE_ATTR(COLOR_BLACK,                  COLOR_WHITE)
#define ATTR_SCROLLBAR   MAKE_ATTR(COLOR_WHITE,                  COLOR_BLACK)
#define ATTR_SCROLLTHUMB MAKE_ATTR(COLOR_BLACK,                  COLOR_WHITE)
