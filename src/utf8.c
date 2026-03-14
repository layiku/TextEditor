/*
 * utf8.c — UTF-8 编解码与显示宽度实现
 */
#include "utf8.h"
#include <stddef.h>  /* NULL */

/* ================================================================
 * 序列长度与解码
 * ================================================================ */

int utf8_seq_len(unsigned char b) {
    if (b < 0x80) return 1;          /* 0xxxxxxx  ASCII */
    if ((b & 0xE0) == 0xC0) return 2; /* 110xxxxx */
    if ((b & 0xF0) == 0xE0) return 3; /* 1110xxxx */
    if ((b & 0xF8) == 0xF0) return 4; /* 11110xxx */
    return 1; /* 续字节或非法：按 1 字节跳过 */
}

int utf8_decode(const char *s, uint32_t *out_cp) {
    unsigned char b0 = (unsigned char)s[0];
    uint32_t cp;
    int len;

    if (b0 < 0x80) {
        cp = b0; len = 1;
    } else if ((b0 & 0xE0) == 0xC0) {
        cp = b0 & 0x1F; len = 2;
    } else if ((b0 & 0xF0) == 0xE0) {
        cp = b0 & 0x0F; len = 3;
    } else if ((b0 & 0xF8) == 0xF0) {
        cp = b0 & 0x07; len = 4;
    } else {
        /* 非法首字节：替换字符，跳过 1 字节 */
        if (out_cp) *out_cp = 0xFFFD;
        return 1;
    }

    /* 读续字节（10xxxxxx），任何不合法字节都降级为替换字符 */
    for (int i = 1; i < len; i++) {
        unsigned char b = (unsigned char)s[i];
        if ((b & 0xC0) != 0x80) {
            if (out_cp) *out_cp = 0xFFFD;
            return 1;
        }
        cp = (cp << 6) | (b & 0x3F);
    }

    if (out_cp) *out_cp = cp;
    return len;
}

int utf8_encode(uint32_t cp, char *out) {
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    } else if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    } else if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    } else {
        out[0] = (char)(0xF0 | (cp >> 18));
        out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }
}

/* ================================================================
 * 显示宽度
 *
 * 全角范围依据 Unicode East Asian Width 属性（W / F = 2 列）。
 * 覆盖主流 CJK 汉字、全角 ASCII、CJK 标点、假名等，足够编辑器日常使用。
 * ================================================================ */

int utf8_cp_width(uint32_t cp) {
    /* CJK Unified Ideographs 4E00-9FFF (最常用汉字) */
    if (cp >= 0x4E00 && cp <= 0x9FFF)  return 2;
    /* CJK Extension A */
    if (cp >= 0x3400 && cp <= 0x4DBF)  return 2;
    /* CJK Extension B-F */
    if (cp >= 0x20000 && cp <= 0x2A6DF) return 2;
    /* CJK Compatibility Ideographs */
    if (cp >= 0xF900 && cp <= 0xFAFF)  return 2;
    /* Hangul Syllables (Korean) */
    if (cp >= 0xAC00 && cp <= 0xD7AF)  return 2;
    /* Hangul Jamo */
    if (cp >= 0x1100 && cp <= 0x11FF)  return 2;
    /* Hiragana, Katakana */
    if (cp >= 0x3040 && cp <= 0x30FF)  return 2;
    /* CJK Symbols and Punctuation */
    if (cp >= 0x3000 && cp <= 0x303F)  return 2;
    /* Fullwidth ASCII variants */
    if (cp >= 0xFF01 && cp <= 0xFF60)  return 2;
    /* Fullwidth brackets etc. */
    if (cp >= 0xFFE0 && cp <= 0xFFE6)  return 2;
    /* Enclosed CJK, CJK Compatibility */
    if (cp >= 0x3200 && cp <= 0x33FF)  return 2;
    /* CJK Radicals, Kangxi */
    if (cp >= 0x2E80 && cp <= 0x2FFF)  return 2;
    /* Bopomofo */
    if (cp >= 0x3100 && cp <= 0x312F)  return 2;
    return 1;
}

/* ================================================================
 * 行内坐标换算
 * ================================================================ */

int utf8_byte_to_col(const char *line, int byte_len, int byte_off) {
    if (byte_off <= 0) return 0;
    if (byte_off > byte_len) byte_off = byte_len;

    int col = 0, pos = 0;
    while (pos < byte_off) {
        uint32_t cp;
        int seq = utf8_decode(line + pos, &cp);
        if (pos + seq > byte_len) break;  /* 残缺序列 */
        col += utf8_cp_width(cp);
        pos += seq;
    }
    return col;
}

int utf8_col_to_byte(const char *line, int byte_len, int col) {
    if (col <= 0) return 0;

    int cur_col = 0, pos = 0;
    while (pos < byte_len) {
        if (cur_col >= col) break;
        uint32_t cp;
        int seq = utf8_decode(line + pos, &cp);
        if (pos + seq > byte_len) break;
        int w = utf8_cp_width(cp);
        if (cur_col + w > col) break;  /* col 落在宽字符内部：返回该字符起始 */
        cur_col += w;
        pos += seq;
    }
    return pos;
}

int utf8_line_display_width(const char *line, int byte_len) {
    return utf8_byte_to_col(line, byte_len, byte_len);
}

/* ================================================================
 * 光标移动
 * ================================================================ */

int utf8_next_char(const char *line, int byte_len, int byte_off) {
    if (byte_off >= byte_len) return byte_len;
    int seq = utf8_seq_len((unsigned char)line[byte_off]);
    /* 保证不超过 byte_len */
    if (byte_off + seq > byte_len) return byte_len;
    return byte_off + seq;
}

int utf8_prev_char(const char *line, int byte_off) {
    if (byte_off <= 0) return 0;
    /* 向前跳过所有续字节（10xxxxxx） */
    int pos = byte_off - 1;
    while (pos > 0 && ((unsigned char)line[pos] & 0xC0) == 0x80)
        pos--;
    return pos;
}
