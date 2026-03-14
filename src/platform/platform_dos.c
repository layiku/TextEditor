/*
 * platform_dos.c — FreeDOS / MS-DOS 平台实现（骨架）
 * 使用 DOS BIOS 中断：INT 10h（显示）、INT 16h（键盘）、INT 33h（鼠标）
 * 仅在 __MSDOS__ 或 __DOS__ 宏定义时编译（OpenWatcom / Turbo C）
 */
#if defined(__MSDOS__) || defined(__DOS__)

#include <dos.h>
#include <string.h>
#include "platform.h"

/* ================================================================
 * DOS 下直接操作 B800:0 显存
 * 每个字符占 2 字节：[字符][属性]，行主序，80 列
 * ================================================================ */
#define VRAM_SEG   0xB800
#define VRAM_COLS  80
#define VRAM_ROWS  25

static unsigned char far *g_vram = NULL;

void plat_init(void) {
    /* 指向 VGA 文本显存 */
    g_vram = (unsigned char far*)MK_FP(VRAM_SEG, 0);

    /* 初始化 INT 33h 鼠标驱动（若存在） */
    union REGS regs;
    regs.x.ax = 0x0000;
    int86(0x33, &regs, &regs);
    /* AX==0xFFFF 表示驱动存在 */
}

void plat_exit(void) {
    /* DOS 下一般不需要还原终端，保持文本模式即可 */
    /* 显示光标 */
    union REGS regs;
    regs.h.ah = 0x01;
    regs.x.cx = 0x0607;  /* 默认光标形状 */
    int86(0x10, &regs, &regs);
}

void plat_get_size(int *rows, int *cols) {
    /* DOS 80×25 文本模式固定尺寸 */
    *rows = VRAM_ROWS;
    *cols = VRAM_COLS;
}

void plat_write_cells(int y, int x, const Cell *cells, int count) {
    unsigned int offset = (unsigned int)(y * VRAM_COLS + x) * 2;
    for (int i = 0; i < count && (x + i) < VRAM_COLS; i++) {
        char c = cells[i].ch ? cells[i].ch : ' ';
        g_vram[offset + i * 2]     = (unsigned char)c;
        g_vram[offset + i * 2 + 1] = cells[i].attr;
    }
}

void plat_set_cursor(int y, int x) {
    union REGS regs;
    regs.h.ah = 0x02;
    regs.h.bh = 0x00;        /* 页号 0 */
    regs.h.dh = (unsigned char)y;
    regs.h.dl = (unsigned char)x;
    int86(0x10, &regs, &regs);
}

void plat_show_cursor(int show) {
    union REGS regs;
    regs.h.ah = 0x01;
    if (show)
        regs.x.cx = 0x0607;  /* 正常光标 */
    else
        regs.x.cx = 0x2000;  /* 隐藏光标（起始行 > 结束行） */
    int86(0x10, &regs, &regs);
}

void plat_clear_screen(uint8_t attr) {
    for (int i = 0; i < VRAM_ROWS * VRAM_COLS; i++) {
        g_vram[i * 2]     = ' ';
        g_vram[i * 2 + 1] = attr;
    }
}

/* ----------------------------------------------------------------
 * 键盘输入（INT 16h）
 * ---------------------------------------------------------------- */

/* INT 16h AH=01：检查有无按键 */
static int kb_available(void) {
    union REGS regs;
    regs.h.ah = 0x01;
    int86(0x16, &regs, &regs);
    return !(regs.x.flags & 0x40);  /* ZF=0 表示有键 */
}

/* INT 16h AH=00：读取按键，返回 AX（AL=ASCII，AH=扫描码） */
static unsigned int kb_read(void) {
    union REGS regs;
    regs.h.ah = 0x00;
    int86(0x16, &regs, &regs);
    return (unsigned int)(regs.x.ax);
}

/* 扫描码 → KEY_* 映射 */
static int scan_to_key(unsigned char scan) {
    switch (scan) {
        case 0x48: return KEY_UP;
        case 0x50: return KEY_DOWN;
        case 0x4B: return KEY_LEFT;
        case 0x4D: return KEY_RIGHT;
        case 0x47: return KEY_HOME;
        case 0x4F: return KEY_END;
        case 0x49: return KEY_PGUP;
        case 0x51: return KEY_PGDN;
        case 0x52: return KEY_INSERT;
        case 0x53: return KEY_DELETE;
        case 0x3B: return KEY_F1;
        case 0x3C: return KEY_F2;
        case 0x3D: return KEY_F3;
        case 0x3E: return KEY_F4;
        case 0x3F: return KEY_F5;
        case 0x40: return KEY_F6;
        case 0x41: return KEY_F7;
        case 0x42: return KEY_F8;
        case 0x43: return KEY_F9;
        case 0x44: return KEY_F10;
        /* F11/F12 扩展扫描码在 DOS 中需要 INT 16h AH=10h */
        default: return KEY_NONE;
    }
}

int plat_poll_event(InputEvent *ev, int timeout_ms) {
    memset(ev, 0, sizeof(InputEvent));

    /* DOS 下简单轮询，不支持精确超时 */
    /* timeout_ms < 0 表示阻塞等待 */
    if (timeout_ms == 0 && !kb_available())
        return 0;

    if (!kb_available()) return 0;

    unsigned int ax = kb_read();
    unsigned char ascii = (unsigned char)(ax & 0xFF);
    unsigned char scan  = (unsigned char)((ax >> 8) & 0xFF);

    ev->type = EVT_KEY;
    ev->mod  = 0;

    if (ascii == 0 || ascii == 0xE0) {
        /* 扩展键（扫描码前缀 0x00 或 0xE0） */
        int k = scan_to_key(scan);
        if (k == KEY_NONE) return 0;
        ev->key = k;
    } else {
        switch (ascii) {
            case 0x1B: ev->key = KEY_ESCAPE;    break;
            case 0x0D: ev->key = KEY_ENTER;     break;
            case 0x08: ev->key = KEY_BACKSPACE; break;
            case 0x09: ev->key = KEY_TAB;       break;
            default:
                if (ascii >= 32) {
                    ev->key = ascii;
                } else {
                    /* Ctrl+字母 */
                    ev->key = ascii - 1 + 'A';
                    ev->mod = MOD_CTRL;
                }
                break;
        }
    }
    return 1;
}

#endif /* __MSDOS__ || __DOS__ */
