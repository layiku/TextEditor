/*
 * regex_simple.c — 轻量正则引擎实现
 * 使用递归回溯 NFA，专为语法高亮规则设计（行长通常 < 512 字节）。
 */
#include <string.h>
#include <stddef.h>
#include "regex_simple.h"

/* ================================================================
 * 内部字符类判断
 * ================================================================ */

static int is_digit(char c)  { return c >= '0' && c <= '9'; }
static int is_alpha(char c)  { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static int is_word(char c)   { return is_alpha(c) || is_digit(c) || c == '_'; }
static int is_space(char c)  { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

/* ================================================================
 * 字符类 [...] 解析
 * pat   — 指向 '[' 之后的字符串
 * c     — 待判断字符
 * out_len — 输出消耗的字节数（包含两端 []）
 * 返回  1=匹配，0=不匹配，-1=格式错误
 * ================================================================ */
static int match_class(const char *pat, char c, int *out_len) {
    const char *p = pat + 1;  /* 跳过 '[' */
    int negate = 0;
    if (*p == '^') { negate = 1; p++; }

    int matched = 0;
    while (*p && *p != ']') {
        if (*(p+1) == '-' && *(p+2) && *(p+2) != ']') {
            /* 范围 a-z */
            if (c >= *p && c <= *(p+2)) matched = 1;
            p += 3;
        } else if (*p == '\\' && *(p+1)) {
            char nc = *(p+1);
            if ((nc == 'd' && is_digit(c))  ||
                (nc == 'w' && is_word(c))   ||
                (nc == 's' && is_space(c))  ||
                (nc == c))
                matched = 1;
            p += 2;
        } else {
            if (*p == c) matched = 1;
            p++;
        }
    }
    if (*p != ']') { *out_len = 0; return -1; }  /* 格式错误 */
    *out_len = (int)(p + 1 - pat);  /* 包含 '[' 和 ']' */
    return negate ? !matched : matched;
}

/* ================================================================
 * 内部核心：匹配器（递归）
 * pat — 当前模式位置
 * txt — 文本
 * len — 文本长度
 * pos — 文本当前位置
 * 返回：匹配后的 pos（>= pos 成功，-1 失败）
 * ================================================================ */
static int match_here(const char *pat, const char *txt, int len, int pos);

/* 判断 txt[pos] 是否与 pat 处的单个模式元素匹配，元素长度写入 *pat_len */
static int match_atom(const char *pat, const char *txt, int len, int pos,
                      int *pat_len) {
    if (!pat[0]) { *pat_len = 0; return 0; }

    if (pat[0] == '\\') {
        *pat_len = 2;
        char nc = pat[1];
        if (pos >= len) return 0;
        char c = txt[pos];
        switch (nc) {
            case 'd': return is_digit(c);
            case 'w': return is_word(c);
            case 's': return is_space(c);
            default:  return (c == nc);
        }
    }
    if (pat[0] == '[') {
        int cl;
        int r = match_class(pat, pos < len ? txt[pos] : '\0', &cl);
        *pat_len = cl;
        return (r == 1) && (pos < len);
    }
    if (pat[0] == '.') {
        *pat_len = 1;
        return pos < len && txt[pos] != '\n';
    }
    /* 字面量 */
    *pat_len = 1;
    return pos < len && txt[pos] == pat[0];
}

/* 递归核心，返回文本结束位置，-1=失败 */
static int match_here(const char *pat, const char *txt, int len, int pos) {
    /* 空模式：匹配成功 */
    if (pat[0] == '\0') return pos;

    /* $ 锚点：必须在末尾 */
    if (pat[0] == '$' && pat[1] == '\0') return (pos == len) ? pos : -1;

    /* \b 词边界 */
    if (pat[0] == '\\' && pat[1] == 'b') {
        int before = (pos > 0)   && is_word(txt[pos - 1]);
        int after  = (pos < len) && is_word(txt[pos]);
        if (before != after)
            return match_here(pat + 2, txt, len, pos);
        return -1;
    }

    /* 预读下一元素以判断量词 */
    int pat_len = 0;
    int atom_ok = match_atom(pat, txt, len, pos, &pat_len);
    (void)atom_ok;  /* 仅用于检测 pat_len */

    if (pat_len > 0) {
        char quant = pat[pat_len];  /* *, +, ? */

        if (quant == '*') {
            /* 贪婪 *：先消耗尽可能多，再回退 */
            int max_pos = pos;
            while (match_atom(pat, txt, len, max_pos, &pat_len) && pat_len > 0)
                max_pos++;
            for (int p2 = max_pos; p2 >= pos; p2--) {
                int r = match_here(pat + pat_len + 1, txt, len, p2);
                if (r >= 0) return r;
            }
            return -1;
        }
        if (quant == '+') {
            /* 贪婪 + */
            if (!match_atom(pat, txt, len, pos, &pat_len)) return -1;
            int max_pos = pos + 1;
            while (match_atom(pat, txt, len, max_pos, &pat_len) && pat_len > 0)
                max_pos++;
            for (int p2 = max_pos; p2 > pos; p2--) {
                int r = match_here(pat + pat_len + 1, txt, len, p2);
                if (r >= 0) return r;
            }
            return -1;
        }
        if (quant == '?') {
            /* 尝试消耗一个，失败则不消耗 */
            if (match_atom(pat, txt, len, pos, &pat_len)) {
                int r = match_here(pat + pat_len + 1, txt, len, pos + 1);
                if (r >= 0) return r;
            }
            return match_here(pat + pat_len + 1, txt, len, pos);
        }
    }

    /* 无量词：精确匹配一次 */
    if (match_atom(pat, txt, len, pos, &pat_len) && pat_len > 0)
        return match_here(pat + pat_len, txt, len, pos + 1);

    return -1;
}

/* ================================================================
 * 公开接口
 * ================================================================ */

int regex_match(const char *pattern, const char *text, int len,
                int pos, int *out_end) {
    if (!pattern || !text) return 0;

    const char *pat = pattern;
    int anchor = (pat[0] == '^');
    if (anchor) pat++;

    int end = match_here(pat, text, len, pos);
    if (end < 0) return 0;
    if (out_end) *out_end = end;
    return 1;
}

int regex_search(const char *pattern, const char *text, int len,
                 int pos, int *out_end) {
    if (!pattern || !text) return -1;

    const char *pat = pattern;
    int anchor = (pat[0] == '^');
    if (anchor) pat++;

    int start = pos;
    do {
        int end = match_here(pat, text, len, start);
        if (end >= 0) {
            if (out_end) *out_end = end;
            return start;
        }
        start++;
    } while (!anchor && start <= len);

    return -1;
}
