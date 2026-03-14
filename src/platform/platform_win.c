/*
 * platform_win.c — Windows 控制台平台实现
 * 使用 Windows Console API（ReadConsoleInput、WriteConsoleOutput 等）
 * 仅在 _WIN32 宏定义时编译
 */
#ifdef _WIN32

#include <windows.h>
/* Windows SDK 在 winuser.h 中定义了 MOD_SHIFT/MOD_ALT（用于 RegisterHotKey），
 * 与 types.h 中的同名宏冲突；先 undef，再由 platform.h→types.h 以我们的值重定义 */
#undef MOD_SHIFT
#undef MOD_ALT
#undef MOD_CONTROL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"

/* ================================================================
 * 内部状态
 * ================================================================ */
static HANDLE g_hout = INVALID_HANDLE_VALUE;  /* 标准输出句柄 */
static HANDLE g_hin  = INVALID_HANDLE_VALUE;  /* 标准输入句柄 */
static DWORD  g_orig_out_mode = 0;            /* 原始输出模式（退出时还原） */
static DWORD  g_orig_in_mode  = 0;            /* 原始输入模式 */
static CONSOLE_CURSOR_INFO g_orig_cursor;     /* 原始光标信息 */

/* ================================================================
 * Ctrl+C 处理
 * ================================================================ */
/* 屏蔽 Ctrl+C/Ctrl+Break 的默认退出行为，
 * 使其能被 ReadConsoleInput 捕获为普通键事件 */
static BOOL WINAPI ctrl_handler(DWORD ctrl_type) {
    /* 返回 TRUE 表示"我已处理"，不交给下一个处理器（不退出） */
    if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_BREAK_EVENT)
        return TRUE;
    return FALSE;
}

/* ================================================================
 * 生命周期
 * ================================================================ */

void plat_init(void) {
    g_hout = GetStdHandle(STD_OUTPUT_HANDLE);
    g_hin  = GetStdHandle(STD_INPUT_HANDLE);

    /* 保存原始模式 */
    GetConsoleMode(g_hout, &g_orig_out_mode);
    GetConsoleMode(g_hin,  &g_orig_in_mode);
    GetConsoleCursorInfo(g_hout, &g_orig_cursor);

    /* 输出模式：关闭自动换行，启用虚拟终端处理（Win10+可选） */
    DWORD out_mode = g_orig_out_mode;
    out_mode &= ~ENABLE_WRAP_AT_EOL_OUTPUT;
    SetConsoleMode(g_hout, out_mode);

    /* 输入模式：关闭行输入和回显，启用鼠标输入和窗口尺寸事件 */
    DWORD in_mode = 0;
    in_mode |= ENABLE_MOUSE_INPUT;
    in_mode |= ENABLE_WINDOW_INPUT;
    in_mode |= ENABLE_EXTENDED_FLAGS;  /* 必须配合 ENABLE_QUICK_EDIT_MODE=0 */
    /* 关闭 QuickEdit（防止鼠标右键粘贴干扰） */
    SetConsoleMode(g_hin, in_mode);

    /* 屏蔽 Ctrl+C 默认退出，使之作为普通键被读取 */
    SetConsoleCtrlHandler(ctrl_handler, TRUE);

    /* 隐藏光标（减少渲染时闪烁，display 层会在合适时机显示） */
    CONSOLE_CURSOR_INFO ci = { 1, FALSE };
    SetConsoleCursorInfo(g_hout, &ci);

    /* 将控制台输入/输出代码页切换为 UTF-8 (65001)。
     * UI 字符串全为 ASCII，编辑区文件内容若为 UTF-8 也可正确显示。
     * 注意：plat_exit 时无需专门还原，Windows 会在进程退出后自动恢复。 */
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

void plat_exit(void) {
    /* 还原控制台状态 */
    SetConsoleMode(g_hout, g_orig_out_mode);
    SetConsoleMode(g_hin,  g_orig_in_mode);
    SetConsoleCursorInfo(g_hout, &g_orig_cursor);
    SetConsoleCtrlHandler(ctrl_handler, FALSE);

    /* 移动光标到底部，确保程序输出不被覆盖 */
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(g_hout, &csbi)) {
        COORD pos = { 0, (SHORT)(csbi.dwSize.Y - 1) };
        SetConsoleCursorPosition(g_hout, pos);
    }
}

/* ================================================================
 * 屏幕尺寸
 * ================================================================ */

void plat_get_size(int *rows, int *cols) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(g_hout, &csbi)) {
        *cols = csbi.srWindow.Right  - csbi.srWindow.Left + 1;
        *rows = csbi.srWindow.Bottom - csbi.srWindow.Top  + 1;
    } else {
        *rows = 25;
        *cols = 80;
    }
}

/* ================================================================
 * 屏幕输出
 * ================================================================ */

/* VGA 属性字节 → Windows WORD 属性（颜色编码相同，可直接使用） */
static WORD attr_to_win(uint8_t attr) {
    return (WORD)attr;
}

void plat_write_cells(int y, int x, const Cell *cells, int count) {
    if (count <= 0) return;

    /* 构建 CHAR_INFO 数组，使用 WriteConsoleOutputW 输出 Unicode */
    CHAR_INFO *ci = (CHAR_INFO*)malloc((size_t)count * sizeof(CHAR_INFO));
    if (!ci) return;

    for (int i = 0; i < count; i++) {
        WCHAR wch = cells[i].ch ? (WCHAR)cells[i].ch : L' ';
        WORD  attr = attr_to_win(cells[i].attr);
        if (cells[i].width == 2) {
            /* 全宽字符主格：需要 LEADING_BYTE 标志 */
            ci[i].Char.UnicodeChar = wch;
            ci[i].Attributes       = attr | COMMON_LVB_LEADING_BYTE;
        } else if (cells[i].width == 0 && i > 0) {
            /* 续格（全宽字符右半）：TRAILING_BYTE 标志，同一码点 */
            ci[i].Char.UnicodeChar = wch;
            ci[i].Attributes       = attr | COMMON_LVB_TRAILING_BYTE;
        } else {
            /* 普通半宽字符 */
            ci[i].Char.UnicodeChar = wch;
            ci[i].Attributes       = attr;
        }
    }

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(g_hout, &csbi);
    SHORT top = csbi.srWindow.Top;

    COORD buf_size  = { (SHORT)count, 1 };
    COORD buf_coord = { 0, 0 };
    SMALL_RECT region = {
        (SHORT)(x),
        (SHORT)(y + top),
        (SHORT)(x + count - 1),
        (SHORT)(y + top)
    };
    WriteConsoleOutputW(g_hout, ci, buf_size, buf_coord, &region);
    free(ci);
}

void plat_set_cursor(int y, int x) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(g_hout, &csbi);
    COORD pos = {
        (SHORT)(x),
        (SHORT)(y + csbi.srWindow.Top)
    };
    SetConsoleCursorPosition(g_hout, pos);
}

void plat_show_cursor(int show) {
    CONSOLE_CURSOR_INFO ci;
    GetConsoleCursorInfo(g_hout, &ci);
    ci.bVisible = show ? TRUE : FALSE;
    SetConsoleCursorInfo(g_hout, &ci);
}

void plat_clear_screen(uint8_t attr) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(g_hout, &csbi);
    int rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    int cols = csbi.srWindow.Right  - csbi.srWindow.Left + 1;

    CHAR_INFO *ci = (CHAR_INFO*)malloc((size_t)cols * sizeof(CHAR_INFO));
    if (!ci) return;
    for (int j = 0; j < cols; j++) {
        ci[j].Char.UnicodeChar = L' ';
        ci[j].Attributes       = attr_to_win(attr);
    }
    COORD buf_size  = { (SHORT)cols, 1 };
    COORD buf_coord = { 0, 0 };
    for (int r = 0; r < rows; r++) {
        SMALL_RECT region = {
            csbi.srWindow.Left,
            (SHORT)(csbi.srWindow.Top + r),
            csbi.srWindow.Right,
            (SHORT)(csbi.srWindow.Top + r)
        };
        WriteConsoleOutputW(g_hout, ci, buf_size, buf_coord, &region);
    }
    free(ci);
}

/* ================================================================
 * 输入
 * ================================================================ */

/* Windows 虚拟键码 → 编辑器虚拟键码 */
static int vk_to_key(WORD vk, DWORD ctrl_state) {
    /* 处理 Ctrl+字母（作为 Ctrl+? 控制字符）*/
    (void)ctrl_state;
    switch (vk) {
        case VK_UP:     return KEY_UP;
        case VK_DOWN:   return KEY_DOWN;
        case VK_LEFT:   return KEY_LEFT;
        case VK_RIGHT:  return KEY_RIGHT;
        case VK_HOME:   return KEY_HOME;
        case VK_END:    return KEY_END;
        case VK_PRIOR:  return KEY_PGUP;
        case VK_NEXT:   return KEY_PGDN;
        case VK_INSERT: return KEY_INSERT;
        case VK_DELETE: return KEY_DELETE;
        case VK_BACK:   return KEY_BACKSPACE;
        case VK_RETURN: return KEY_ENTER;
        case VK_ESCAPE: return KEY_ESCAPE;
        case VK_TAB:    return KEY_TAB;
        case VK_F1:     return KEY_F1;
        case VK_F2:     return KEY_F2;
        case VK_F3:     return KEY_F3;
        case VK_F4:     return KEY_F4;
        case VK_F5:     return KEY_F5;
        case VK_F6:     return KEY_F6;
        case VK_F7:     return KEY_F7;
        case VK_F8:     return KEY_F8;
        case VK_F9:     return KEY_F9;
        case VK_F10:    return KEY_F10;
        case VK_F11:    return KEY_F11;
        case VK_F12:    return KEY_F12;
        default:        return KEY_NONE;
    }
}

/* 修饰键状态 → MOD_* 标志 */
static int ctrl_state_to_mod(DWORD ctrl_state) {
    int mod = 0;
    if (ctrl_state & (SHIFT_PRESSED))
        mod |= MOD_SHIFT;
    if (ctrl_state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
        mod |= MOD_CTRL;
    if (ctrl_state & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED))
        mod |= MOD_ALT;
    return mod;
}

int plat_poll_event(InputEvent *ev, int timeout_ms) {
    memset(ev, 0, sizeof(InputEvent));

    /* 等待有输入事件（带超时） */
    DWORD wait_ms = (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms;
    DWORD result = WaitForSingleObject(g_hin, wait_ms);
    if (result != WAIT_OBJECT_0) return 0;  /* 超时或错误 */

    INPUT_RECORD ir;
    DWORD nread = 0;
    if (!ReadConsoleInputW(g_hin, &ir, 1, &nread) || nread == 0)
        return 0;

    switch (ir.EventType) {

    case KEY_EVENT: {
        KEY_EVENT_RECORD *ke = &ir.Event.KeyEvent;
        if (!ke->bKeyDown) return 0;  /* 只处理按下事件 */

        ev->type = EVT_KEY;
        ev->mod  = ctrl_state_to_mod(ke->dwControlKeyState);

        /* 优先处理特殊键 */
        int special = vk_to_key(ke->wVirtualKeyCode, ke->dwControlKeyState);
        if (special != KEY_NONE) {
            ev->key = special;
        } else if (ke->uChar.UnicodeChar >= 0x20) {
            /* 可打印 Unicode 字符（BMP）：key 存码点 */
            ev->key = (int)(unsigned short)ke->uChar.UnicodeChar;
        } else if (ke->uChar.UnicodeChar > 0) {
            /* Ctrl+字母（ASCII 1–31）：key 存对应字母（A–Z） */
            ev->key = ke->uChar.UnicodeChar + 'A' - 1;
            ev->mod |= MOD_CTRL;
        } else {
            return 0;  /* 无法识别，跳过 */
        }
        return 1;
    }

    case MOUSE_EVENT: {
        MOUSE_EVENT_RECORD *me = &ir.Event.MouseEvent;
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(g_hout, &csbi);

        ev->mx = me->dwMousePosition.X - csbi.srWindow.Left;
        ev->my = me->dwMousePosition.Y - csbi.srWindow.Top;
        ev->mod = ctrl_state_to_mod(me->dwControlKeyState);

        if (me->dwEventFlags == 0) {
            /* 按键按下/释放事件 */
            if (me->dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) {
                ev->type = EVT_MOUSE_DOWN;
                ev->mbutton = 0;
            } else if (me->dwButtonState & RIGHTMOST_BUTTON_PRESSED) {
                ev->type = EVT_MOUSE_DOWN;
                ev->mbutton = 1;
            } else {
                ev->type = EVT_MOUSE_UP;
                ev->mbutton = 0;
            }
        } else if (me->dwEventFlags & MOUSE_MOVED) {
            ev->type = EVT_MOUSE_MOVE;
            ev->mbutton = (me->dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) ? 0 : -1;
        } else if (me->dwEventFlags & MOUSE_WHEELED) {
            /* Windows: HIWORD(dwButtonState) 为滚动量；正值=向上（远离用户） */
            SHORT delta = (SHORT)HIWORD(me->dwButtonState);
            ev->type    = EVT_SCROLL;
            /* 正值→视口向上（文档向前），用正 mscroll 表示；负值→向下 */
            ev->mscroll = (delta > 0) ? 3 : -3;
        } else {
            return 0;
        }
        return 1;
    }

    case WINDOW_BUFFER_SIZE_EVENT:
        ev->type = EVT_RESIZE;
        return 1;

    default:
        return 0;
    }
}

#endif /* _WIN32 */
