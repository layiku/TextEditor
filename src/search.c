/*
 * search.c — 查找与替换实现
 * 使用 strstr / strcasestr 做行内搜索，逐行扫描
 */
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "search.h"
#include "util.h"

/* ================================================================
 * 内部状态
 * ================================================================ */
#define MAX_SEARCH_LEN 256

static char g_find_str[MAX_SEARCH_LEN]    = "";
static char g_replace_str[MAX_SEARCH_LEN] = "";
static int  g_options = SEARCH_CASE_SENSITIVE;

/* ================================================================
 * 接口实现
 * ================================================================ */

void search_init(const char *find_str, const char *replace_str, int options) {
    if (find_str)    strncpy(g_find_str,    find_str,    MAX_SEARCH_LEN - 1);
    if (replace_str) strncpy(g_replace_str, replace_str, MAX_SEARCH_LEN - 1);
    g_find_str[MAX_SEARCH_LEN - 1]    = '\0';
    g_replace_str[MAX_SEARCH_LEN - 1] = '\0';
    g_options = options;
}

const char* search_get_find_str(void)    { return g_find_str; }
const char* search_get_replace_str(void) { return g_replace_str; }
int         search_get_options(void)     { return g_options; }

/* 在行 text 上从偏移 col_start 开始查找 g_find_str
 * 找到返回列偏移（相对行头），未找到返回 -1 */
static int find_in_line(const char *text, int col_start) {
    if (!text || !g_find_str[0]) return -1;
    const char *haystack = text + col_start;
    const char *found;

    if (g_options & SEARCH_CASE_SENSITIVE)
        found = strstr(haystack, g_find_str);
    else
        found = str_istr(haystack, g_find_str);

    if (!found) return -1;
    return (int)(found - text);
}

/* 反向版本：在 text 的 [0, col_end) 内最后一次出现位置 */
static int rfind_in_line(const char *text, int col_end) {
    if (!text || !g_find_str[0]) return -1;
    int flen  = (int)strlen(g_find_str);
    int last  = -1;
    int start = 0;

    while (start + flen <= col_end) {
        int found = find_in_line(text, start);
        if (found < 0 || found >= col_end) break;
        last  = found;
        start = found + 1;
    }
    return last;
}

int search_next(Document *doc, int from_row, int from_col,
                int *out_row, int *out_col) {
    if (!g_find_str[0]) return -1;
    int n    = document_line_count(doc);
    int flen = (int)strlen(g_find_str);
    bool wrapped = false;

    int start_row = from_row;
    int start_col = from_col;

    for (int pass = 0; pass < 2; pass++) {
        for (int r = start_row; r < n; r++) {
            const char *line = document_get_line(doc, r);
            int col_from = (r == start_row && pass == 0) ? start_col : 0;
            int found    = find_in_line(line, col_from);
            if (found >= 0) {
                if (out_row) *out_row = r;
                if (out_col) *out_col = found;
                return flen;
            }
        }
        /* 第一圈未找到，从文档开头再来一遍（回绕） */
        if (pass == 0) {
            start_row = 0;
            start_col = 0;
            wrapped   = true;
        }
    }
    (void)wrapped;
    return -1;
}

int search_prev(Document *doc, int from_row, int from_col,
                int *out_row, int *out_col) {
    if (!g_find_str[0]) return -1;
    int n    = document_line_count(doc);
    int flen = (int)strlen(g_find_str);

    /* 从 from 向上搜索，先检查当前行的前半段 */
    for (int pass = 0; pass < 2; pass++) {
        int start_row = (pass == 0) ? from_row : n - 1;
        for (int r = start_row; r >= 0; r--) {
            const char *line = document_get_line(doc, r);
            int line_len = document_get_line_len(doc, r);
            /* 搜索范围：行的 [0, col_end)；当前行只搜 from_col 之前 */
            int col_end = (r == from_row && pass == 0) ? from_col : line_len;
            int found   = rfind_in_line(line, col_end);
            if (found >= 0) {
                if (out_row) *out_row = r;
                if (out_col) *out_col = found;
                return flen;
            }
        }
        /* 回绕到末尾 */
        from_row = n - 1;
        from_col = document_get_line_len(doc, n - 1);
    }
    return -1;
}

int search_replace_current(Document *doc, int row, int col, int match_len) {
    if (match_len <= 0 && g_replace_str[0] == '\0') return 0;

    int rlen = (int)strlen(g_replace_str);

    /* 删除匹配文本 */
    char trash[MAX_SEARCH_LEN * 2];
    document_delete_range(doc, row, col, row, col + match_len,
                          trash, (int)sizeof(trash));

    /* 插入替换文本 */
    if (rlen > 0) {
        int new_row, new_col;
        document_insert_text(doc, row, col, g_replace_str, rlen,
                             &new_row, &new_col);
    }
    return 0;
}

int search_replace_all(Document *doc) {
    if (!g_find_str[0]) return 0;
    int count = 0;
    int row = 0, col = 0;
    int out_row, out_col;
    int match_len;
    int flen = (int)strlen(g_find_str);
    int rlen = (int)strlen(g_replace_str);

    /* 简单策略：从头开始，每次替换后从替换后光标位置继续（防止无限循环） */
    while ((match_len = search_next(doc, row, col, &out_row, &out_col)) > 0) {
        search_replace_current(doc, out_row, out_col, flen);
        count++;
        /* 替换后，光标移到替换文本结尾处继续查找 */
        row = out_row;
        col = out_col + rlen;
        /* 防止死循环（替换串包含查找串且长度非零） */
        if (count > 100000) break;
    }
    return count;
}
