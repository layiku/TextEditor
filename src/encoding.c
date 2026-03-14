/*
 * encoding.c — GBK/UTF-8 编码检测与互转
 *
 * Windows 平台：使用 MultiByteToWideChar / WideCharToMultiByte（CP_936），
 *   完整支持所有 21,003 个 GBK 字符，无需嵌入查找表。
 * 其他平台：仅编码检测 + UTF-8 透传（GBK 转换返回原始字节）。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "encoding.h"
#include "utf8.h"

/* ================================================================
 * 编码检测
 * 检测规则（按优先级）：
 *   1. UTF-8 BOM (EF BB BF) → ENC_UTF8
 *   2. 全部字节均可解析为合法 UTF-8 → ENC_UTF8
 *   3. 否则（含 GBK 高字节）→ ENC_GBK
 * ================================================================ */

/* 验证 buf[0..len) 是否是合法 UTF-8（无 BOM 前缀） */
static int is_valid_utf8(const char *buf, int len) {
    int pos = 0;
    while (pos < len) {
        unsigned char b = (unsigned char)buf[pos];
        int seq;
        if (b < 0x80) {
            seq = 1;
        } else if ((b & 0xE0) == 0xC0) {
            seq = 2;
        } else if ((b & 0xF0) == 0xE0) {
            seq = 3;
        } else if ((b & 0xF8) == 0xF0) {
            seq = 4;
        } else {
            return 0;  /* 非法首字节 */
        }
        if (pos + seq > len) return 0;  /* 截断序列 */
        for (int i = 1; i < seq; i++) {
            if (((unsigned char)buf[pos + i] & 0xC0) != 0x80)
                return 0;  /* 非法续字节 */
        }
        pos += seq;
    }
    return 1;
}

FileEncoding encoding_detect(const char *buf, int len) {
    if (!buf || len <= 0) return ENC_UTF8;

    /* BOM 检测 */
    if (len >= 3 &&
        (unsigned char)buf[0] == 0xEF &&
        (unsigned char)buf[1] == 0xBB &&
        (unsigned char)buf[2] == 0xBF)
        return ENC_UTF8;

    /* 尝试完整解析为 UTF-8 */
    if (is_valid_utf8(buf, len)) return ENC_UTF8;

    /* 含非法 UTF-8 序列 → 假定 GBK */
    return ENC_GBK;
}

/* ================================================================
 * 编码转换
 * ================================================================ */

#ifdef _WIN32
#include <windows.h>

int gbk_to_utf8_buf(const char *in, int in_len, char *out, int out_cap) {
    if (in_len <= 0) { if (out_cap > 0) out[0] = '\0'; return 0; }

    /* Step 1: GBK (CP936) → UTF-16 */
    int wlen = MultiByteToWideChar(936, 0, in, in_len, NULL, 0);
    if (wlen <= 0) return -1;

    WCHAR *wbuf = (WCHAR*)malloc((size_t)(wlen + 1) * sizeof(WCHAR));
    if (!wbuf) return -1;
    MultiByteToWideChar(936, 0, in, in_len, wbuf, wlen);
    wbuf[wlen] = 0;

    /* Step 2: UTF-16 → UTF-8（手工编码，保持跨平台） */
    int out_pos = 0;
    for (int i = 0; i < wlen; i++) {
        uint32_t cp = (uint32_t)wbuf[i];
        /* 处理 UTF-16 代理对（Surrogate Pair） */
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < wlen) {
            uint32_t hi = cp, lo = (uint32_t)wbuf[i + 1];
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000 + ((hi - 0xD800) << 10) + (lo - 0xDC00);
                i++;
            }
        }
        char tmp[4];
        int seq = utf8_encode(cp, tmp);
        if (out_pos + seq >= out_cap) { free(wbuf); return -1; }
        memcpy(out + out_pos, tmp, (size_t)seq);
        out_pos += seq;
    }
    if (out_pos < out_cap) out[out_pos] = '\0';
    free(wbuf);
    return out_pos;
}

int utf8_to_gbk_buf(const char *in, int in_len, char *out, int out_cap) {
    if (in_len <= 0) { if (out_cap > 0) out[0] = '\0'; return 0; }

    /* Step 1: UTF-8 → UTF-16 */
    int wlen = MultiByteToWideChar(CP_UTF8, 0, in, in_len, NULL, 0);
    if (wlen <= 0) return -1;

    WCHAR *wbuf = (WCHAR*)malloc((size_t)(wlen + 1) * sizeof(WCHAR));
    if (!wbuf) return -1;
    MultiByteToWideChar(CP_UTF8, 0, in, in_len, wbuf, wlen);
    wbuf[wlen] = 0;

    /* Step 2: UTF-16 → GBK (CP936)
     * 无法映射的字符用 '?' 代替 */
    int gbk_len = WideCharToMultiByte(936, WC_NO_BEST_FIT_CHARS,
                                       wbuf, wlen, NULL, 0, "?", NULL);
    if (gbk_len <= 0 || gbk_len >= out_cap) {
        free(wbuf);
        return -1;
    }
    WideCharToMultiByte(936, WC_NO_BEST_FIT_CHARS,
                        wbuf, wlen, out, out_cap - 1, "?", NULL);
    out[gbk_len] = '\0';
    free(wbuf);
    return gbk_len;
}

#else
/* ================================================================
 * 非 Windows 平台：仅 UTF-8 透传（不执行 GBK 转换）
 * 如需跨平台 GBK 支持，可在此处嵌入 CP936 查找表。
 * ================================================================ */

int gbk_to_utf8_buf(const char *in, int in_len, char *out, int out_cap) {
    /* 非 Windows：原样复制（调用方应在加载前检测编码） */
    if (in_len >= out_cap) return -1;
    memcpy(out, in, (size_t)in_len);
    out[in_len] = '\0';
    return in_len;
}

int utf8_to_gbk_buf(const char *in, int in_len, char *out, int out_cap) {
    if (in_len >= out_cap) return -1;
    memcpy(out, in, (size_t)in_len);
    out[in_len] = '\0';
    return in_len;
}

#endif /* _WIN32 */
