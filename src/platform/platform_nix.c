/*
 * platform_nix.c — Linux/Unix 终端平台实现
 * 使用 POSIX termios（原始模式）+ ANSI 转义序列
 * 无第三方依赖（不使用 ncurses）
 * 仅在非 _WIN32、非 __MSDOS__ 时编译
 */
#if !defined(_WIN32) && !defined(__MSDOS__)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <signal.h>
#include "platform.h"

/* ================================================================
 * 内部状态
 * ================================================================ */
static struct termios g_orig_termios;       /* 原始 termios，退出时还原 */
static int            g_mouse_enabled = 0;  /* 是否成功启用鼠标 */

/* ANSI 颜色映射：VGA 前景色 0-7 → ANSI 30-37（亮色 +60 or 用 1;3x） */
static const int fg_ansi[16] = {
    30, 34, 32, 36, 31, 35, 33, 37,   /* 暗色 0-7 */
    90, 94, 92, 96, 91, 95, 93, 97    /* 亮色 8-15 */
};
/* VGA 背景色 0-7 → ANSI 40-47 */
static const int bg_ansi[8] = { 40, 44, 42, 46, 41, 45, 43, 47 };

/* ================================================================
 * 生命周期
 * ================================================================ */

void plat_init(void) {
    /* 保存原始 termios */
    tcgetattr(STDIN_FILENO, &g_orig_termios);

    /* 进入原始模式：关闭行缓冲、回显、信号（屏蔽 Ctrl+C 中断） */
    struct termios raw = g_orig_termios;
    raw.c_lflag &= ~(unsigned)(ICANON | ECHO | ISIG | IEXTEN);
    raw.c_iflag &= ~(unsigned)(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_cflag |= CS8;
    raw.c_oflag &= ~(unsigned)OPOST;
    raw.c_cc[VMIN]  = 0;   /* read() 非阻塞 */
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    /* 尝试启用 ANSI X10 鼠标协议 */
    write(STDOUT_FILENO, "\033[?1000h", 8);  /* 按键事件 */
    write(STDOUT_FILENO, "\033[?1002h", 8);  /* 按下+移动事件 */
    g_mouse_enabled = 1;

    /* 隐藏光标（减少渲染闪烁） */
    write(STDOUT_FILENO, "\033[?25l", 6);

    /* 进入备用屏幕（恢复时还原原始终端内容） */
    write(STDOUT_FILENO, "\033[?1049h", 8);

    /* 清屏 */
    write(STDOUT_FILENO, "\033[2J", 4);
    write(STDOUT_FILENO, "\033[H",  3);
}

void plat_exit(void) {
    /* 关闭鼠标报告 */
    if (g_mouse_enabled) {
        write(STDOUT_FILENO, "\033[?1002l", 8);
        write(STDOUT_FILENO, "\033[?1000l", 8);
    }

    /* 显示光标 */
    write(STDOUT_FILENO, "\033[?25h", 6);

    /* 退出备用屏幕 */
    write(STDOUT_FILENO, "\033[?1049l", 8);

    /* 还原 termios */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
}

/* ================================================================
 * 屏幕尺寸
 * ================================================================ */

void plat_get_size(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
    } else {
        /* 回退：读取环境变量 */
        const char *r = getenv("LINES");
        const char *c = getenv("COLUMNS");
        *rows = r ? atoi(r) : 25;
        *cols = c ? atoi(c) : 80;
    }
}

/* ================================================================
 * 屏幕输出
 * ================================================================ */

/* 将 VGA attr 转换为 ANSI 转义序列，写入缓冲 */
static int attr_to_ansi(uint8_t attr, char *buf) {
    int fg  = attr & 0x0F;
    int bg  = (attr >> 4) & 0x07;
    /* ANSI：\033[前景;背景m */
    return snprintf(buf, 32, "\033[%d;%dm", fg_ansi[fg], bg_ansi[bg]);
}

/* 当前属性缓存，避免重复发送相同属性序列 */
static uint8_t g_cur_attr = 0xFF;  /* 0xFF = 未初始化 */

void plat_write_cells(int y, int x, const Cell *cells, int count) {
    if (count <= 0) return;

    /* 移动光标到 (y+1, x+1)（ANSI 1-based） */
    char buf[64];
    int  n = snprintf(buf, sizeof(buf), "\033[%d;%dH", y + 1, x + 1);
    write(STDOUT_FILENO, buf, (size_t)n);

    /* 逐个单元格输出（合并属性相同的字符以减少转义序列数量） */
    for (int i = 0; i < count; i++) {
        if (cells[i].attr != g_cur_attr) {
            char abuf[32];
            int alen = attr_to_ansi(cells[i].attr, abuf);
            write(STDOUT_FILENO, abuf, (size_t)alen);
            g_cur_attr = cells[i].attr;
        }
        char c = cells[i].ch ? cells[i].ch : ' ';
        write(STDOUT_FILENO, &c, 1);
    }
}

void plat_set_cursor(int y, int x) {
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "\033[%d;%dH", y + 1, x + 1);
    write(STDOUT_FILENO, buf, (size_t)n);
}

void plat_show_cursor(int show) {
    if (show)
        write(STDOUT_FILENO, "\033[?25h", 6);
    else
        write(STDOUT_FILENO, "\033[?25l", 6);
}

void plat_clear_screen(uint8_t attr) {
    char buf[32];
    int bg = (attr >> 4) & 0x07;
    int n = snprintf(buf, sizeof(buf), "\033[%dm\033[2J\033[H", bg_ansi[bg]);
    write(STDOUT_FILENO, buf, (size_t)n);
    g_cur_attr = 0xFF;  /* 重置属性缓存 */
}

/* ================================================================
 * 输入解析
 * ================================================================ */

/* 解析 ANSI 转义序列，填入 ev，返回消耗的字节数 */
static int parse_escape(const char *buf, int len, InputEvent *ev) {
    if (len < 2) return 0;  /* 序列不完整 */

    if (buf[1] == '[') {
        /* CSI 序列 */
        if (len < 3) return 0;

        /* 鼠标事件：\033[M<btn><x><y>（X10 协议，3 字节参数） */
        if (buf[2] == 'M' && len >= 6) {
            int btn   = (unsigned char)buf[3] - 32;
            int mx    = (unsigned char)buf[4] - 33;  /* 1-based → 0-based */
            int my    = (unsigned char)buf[5] - 33;
            ev->mx = mx;
            ev->my = my;
            if (btn & 64) {
                /* 滚轮，暂不处理 */
                return 6;
            }
            int btn_id = btn & 3;
            if (btn_id == 3) {
                ev->type    = EVT_MOUSE_UP;
                ev->mbutton = 0;
            } else {
                ev->type    = EVT_MOUSE_DOWN;
                ev->mbutton = btn_id;
            }
            ev->mod = 0;
            if (btn & 4)  ev->mod |= MOD_SHIFT;
            if (btn & 8)  ev->mod |= MOD_ALT;
            if (btn & 16) ev->mod |= MOD_CTRL;
            return 6;
        }

        /* 方向键和功能键 */
        if (len >= 3) {
            ev->type = EVT_KEY;
            ev->mod  = 0;
            switch (buf[2]) {
                case 'A': ev->key = KEY_UP;    return 3;
                case 'B': ev->key = KEY_DOWN;  return 3;
                case 'C': ev->key = KEY_RIGHT; return 3;
                case 'D': ev->key = KEY_LEFT;  return 3;
                case 'H': ev->key = KEY_HOME;  return 3;
                case 'F': ev->key = KEY_END;   return 3;
            }
        }

        /* \033[n~ 形式 */
        if (len >= 4 && buf[len-1] == '~') {
            int code = atoi(buf + 2);
            ev->type = EVT_KEY;
            ev->mod  = 0;
            switch (code) {
                case 1: case 7: ev->key = KEY_HOME;   return len;
                case 2:         ev->key = KEY_INSERT; return len;
                case 3:         ev->key = KEY_DELETE; return len;
                case 4: case 8: ev->key = KEY_END;    return len;
                case 5:         ev->key = KEY_PGUP;   return len;
                case 6:         ev->key = KEY_PGDN;   return len;
                case 11: ev->key = KEY_F1;  return len;
                case 12: ev->key = KEY_F2;  return len;
                case 13: ev->key = KEY_F3;  return len;
                case 14: ev->key = KEY_F4;  return len;
                case 15: ev->key = KEY_F5;  return len;
                case 17: ev->key = KEY_F6;  return len;
                case 18: ev->key = KEY_F7;  return len;
                case 19: ev->key = KEY_F8;  return len;
                case 20: ev->key = KEY_F9;  return len;
                case 21: ev->key = KEY_F10; return len;
                case 23: ev->key = KEY_F11; return len;
                case 24: ev->key = KEY_F12; return len;
            }
        }

        /* Shift+方向键 \033[1;2A 等 */
        if (len >= 6 && buf[2] == '1' && buf[3] == ';') {
            int mod_code = buf[4] - '0';
            ev->type = EVT_KEY;
            ev->mod  = 0;
            if (mod_code == 2) ev->mod |= MOD_SHIFT;
            if (mod_code == 3) ev->mod |= MOD_ALT;
            if (mod_code == 5) ev->mod |= MOD_CTRL;
            if (mod_code == 6) ev->mod |= MOD_CTRL | MOD_SHIFT;
            switch (buf[5]) {
                case 'A': ev->key = KEY_UP;    return 6;
                case 'B': ev->key = KEY_DOWN;  return 6;
                case 'C': ev->key = KEY_RIGHT; return 6;
                case 'D': ev->key = KEY_LEFT;  return 6;
            }
        }
    } else if (buf[1] == 'O') {
        /* SS3 序列（某些终端用于功能键） */
        if (len >= 3) {
            ev->type = EVT_KEY;
            ev->mod  = 0;
            switch (buf[2]) {
                case 'A': ev->key = KEY_UP;    return 3;
                case 'B': ev->key = KEY_DOWN;  return 3;
                case 'C': ev->key = KEY_RIGHT; return 3;
                case 'D': ev->key = KEY_LEFT;  return 3;
                case 'H': ev->key = KEY_HOME;  return 3;
                case 'F': ev->key = KEY_END;   return 3;
                case 'P': ev->key = KEY_F1;    return 3;
                case 'Q': ev->key = KEY_F2;    return 3;
                case 'R': ev->key = KEY_F3;    return 3;
                case 'S': ev->key = KEY_F4;    return 3;
            }
        }
    } else if (len >= 2) {
        /* Alt+字符：\033c */
        ev->type = EVT_KEY;
        ev->mod  = MOD_ALT;
        ev->key  = (unsigned char)buf[1];
        return 2;
    }

    return 0;  /* 无法识别，跳过 */
}

int plat_poll_event(InputEvent *ev, int timeout_ms) {
    memset(ev, 0, sizeof(InputEvent));

    /* 使用 select 等待输入（支持超时） */
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    struct timeval tv, *ptv = NULL;
    if (timeout_ms >= 0) {
        tv.tv_sec  = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        ptv = &tv;
    }

    int ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, ptv);
    if (ret <= 0) return 0;  /* 超时或错误 */

    /* 读取最多 32 字节（转义序列最长约 10 字节） */
    char buf[32];
    ssize_t nread = read(STDIN_FILENO, buf, sizeof(buf) - 1);
    if (nread <= 0) return 0;
    buf[nread] = '\0';

    /* 处理 ESC 开头的序列 */
    if (buf[0] == '\033' && nread > 1) {
        int consumed = parse_escape(buf, (int)nread, ev);
        if (consumed > 0) return 1;
        /* 无法解析，当成 ESC 键 */
        ev->type = EVT_KEY;
        ev->key  = KEY_ESCAPE;
        ev->mod  = 0;
        return 1;
    }

    /* 单独 ESC 键 */
    if (buf[0] == '\033') {
        ev->type = EVT_KEY;
        ev->key  = KEY_ESCAPE;
        ev->mod  = 0;
        return 1;
    }

    ev->type = EVT_KEY;
    ev->mod  = 0;

    /* Ctrl+字母（ASCII 1-26） */
    if (buf[0] >= 1 && buf[0] <= 26) {
        ev->key = buf[0] - 1 + 'A';   /* 还原对应字母 */
        ev->mod = MOD_CTRL;
        /* 注意：Ctrl+C 被 termios ~ISIG 屏蔽，会到达这里 */
        return 1;
    }

    /* 特殊控制字符 */
    switch ((unsigned char)buf[0]) {
        case 127:  ev->key = KEY_BACKSPACE; return 1;
        case '\n':
        case '\r': ev->key = KEY_ENTER;     return 1;
        case '\t': ev->key = KEY_TAB;       return 1;
    }

    /* 普通可打印字符 */
    if ((unsigned char)buf[0] >= 32) {
        ev->key = (unsigned char)buf[0];
        return 1;
    }

    return 0;
}

#endif /* !_WIN32 && !__MSDOS__ */
