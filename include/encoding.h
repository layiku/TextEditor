/*
 * encoding.h — 文件编码检测与 GBK/UTF-8 互转
 *
 * 设计：
 *   - 内部存储始终使用 UTF-8（Phase 5 结论）
 *   - 加载时自动检测编码，GBK 则转为 UTF-8 存入 Document
 *   - 保存时按原始编码写回（或强制 UTF-8）
 *   - GBK ↔ Unicode 映射采用 CP936 标准，静态查找表 + 二分搜索
 */
#pragma once

#include <stdint.h>

/* ================================================================
 * 编码枚举
 * ================================================================ */
typedef enum {
    ENC_UTF8 = 0,  /* UTF-8（含 BOM 或无 BOM） */
    ENC_GBK  = 1,  /* GBK / GB2312 / CP936 */
} FileEncoding;

/* ================================================================
 * 编码检测
 * 启发式：先查 BOM，再验证 UTF-8 合法性，否则归为 GBK
 * ================================================================ */
FileEncoding encoding_detect(const char *buf, int len);

/* ================================================================
 * 编码转换
 * 返回值：实际写入 out 的字节数（不含 \0）；-1 = out 缓冲区不足
 * out 缓冲区：调用方负责分配，建议 out_cap >= in_len * 3 + 1
 * ================================================================ */

/* GBK 字节流 → UTF-8 字节流（无 BOM） */
int gbk_to_utf8_buf(const char *in, int in_len, char *out, int out_cap);

/* UTF-8 字节流 → GBK 字节流
 * 无法映射的 Unicode 码点用 '?' 代替 */
int utf8_to_gbk_buf(const char *in, int in_len, char *out, int out_cap);
