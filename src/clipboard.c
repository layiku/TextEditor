/*
 * clipboard.c — 内部剪贴板实现
 */
#include <stdlib.h>
#include <string.h>
#include "clipboard.h"
#include "util.h"

static char *g_clip_buf = NULL;  /* 剪贴板内容缓冲区 */
static int   g_clip_len = 0;    /* 有效字节数 */

void clipboard_init(void) {
    g_clip_buf = NULL;
    g_clip_len = 0;
}

void clipboard_exit(void) {
    free(g_clip_buf);
    g_clip_buf = NULL;
    g_clip_len = 0;
}

void clipboard_set(const char *text, int len) {
    free(g_clip_buf);
    if (!text || len <= 0) {
        g_clip_buf = NULL;
        g_clip_len = 0;
        return;
    }
    g_clip_buf = (char*)safe_malloc((size_t)(len + 1));
    memcpy(g_clip_buf, text, (size_t)len);
    g_clip_buf[len] = '\0';
    g_clip_len = len;
}

const char* clipboard_get(int *out_len) {
    if (out_len) *out_len = g_clip_len;
    return g_clip_buf;
}

int clipboard_is_empty(void) {
    return (g_clip_buf == NULL || g_clip_len == 0);
}
