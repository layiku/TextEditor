/*
 * util.h — 通用工具函数声明
 * 内存封装、字符串处理、路径工具，无平台特定依赖（跨平台条件编译在 .c 中）
 */
#pragma once

#include <stddef.h>
#include <stdbool.h>

/* ================================================================
 * 内存分配封装（失败时打印错误并 exit(1)，避免到处判断 NULL）
 * ================================================================ */
void* safe_malloc(size_t size);
void* safe_realloc(void *ptr, size_t size);
char* safe_strdup(const char *s);

/* ================================================================
 * 字符串工具
 * ================================================================ */
/* 去除行尾的 \r、\n（就地修改） */
void str_trim_crlf(char *s);

/* 判断 s 是否以 prefix 开头 */
int str_starts_with(const char *s, const char *prefix);

/* 大小写无关比较（类似 strcasecmp）*/
int str_icmp(const char *a, const char *b);

/* 大小写无关子串查找，返回首次出现位置的指针，未找到返回 NULL */
const char* str_istr(const char *haystack, const char *needle);

/* ================================================================
 * 路径工具
 * ================================================================ */
/* 判断文件是否存在（普通文件，非目录） */
bool file_exists(const char *path);

/* 从完整路径提取目录部分（如 "/a/b/c.txt" → "/a/b"） */
void path_get_dir(const char *path, char *out, int out_size);

/* 拼接目录与文件名 */
void path_join(const char *dir, const char *name, char *out, int out_size);

/* 获取当前可执行文件所在目录 */
void get_exe_dir(char *out, int out_size);

/* 获取用户配置目录（Windows: %APPDATA%，Unix: $HOME） */
void get_user_config_dir(char *out, int out_size);

/* ================================================================
 * 目录枚举
 * ================================================================ */
#define DIR_ENTRY_MAX_NAME 256

typedef struct {
    char name[DIR_ENTRY_MAX_NAME]; /* 条目名称（不含路径） */
    bool is_dir;                   /* true = 子目录，false = 普通文件 */
} DirEntry;

/* 读取 dir 目录下所有条目（含 ".." 返回上级），写入 out[0..max_count)。
 * 排序规则：".." 置首，其余目录按名称升序，再文件按名称升序。
 * 返回实际写入的条目数。 */
int dir_read_entries(const char *dir, DirEntry *out, int max_count);

/* 判断路径是否为目录 */
bool path_is_dir(const char *path);

/* ================================================================
 * 数值工具宏
 * ================================================================ */
#define MAX(a, b)        ((a) > (b) ? (a) : (b))
#define MIN(a, b)        ((a) < (b) ? (a) : (b))
#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))
