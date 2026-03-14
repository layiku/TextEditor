/*
 * search.h — 查找与替换接口
 */
#pragma once

#include <stdbool.h>
#include "document.h"

/* 查找选项标志 */
#define SEARCH_CASE_SENSITIVE  0x01  /* 区分大小写 */
#define SEARCH_WHOLE_WORD      0x02  /* 全词匹配（可选，初期可不实现） */

/* 初始化查找状态（每次打开查找对话框时调用） */
void search_init(const char *find_str, const char *replace_str, int options);

/* 获取当前查找字符串 */
const char* search_get_find_str(void);

/* 获取当前替换字符串 */
const char* search_get_replace_str(void);

/* 获取当前选项 */
int search_get_options(void);

/*
 * 从 (from_row, from_col) 开始向前查找（从光标向文档末尾方向）
 * 找到时将匹配位置写入 *out_row, *out_col，返回匹配长度；
 * 未找到返回 -1；若到达文档末尾自动回绕到开头继续查找一遍。
 */
int search_next(Document *doc, int from_row, int from_col,
                int *out_row, int *out_col);

/*
 * 从 (from_row, from_col) 开始向后查找（从光标向文档开头方向）
 * 找到时返回匹配长度，写入 *out_row, *out_col；未找到返回 -1。
 */
int search_prev(Document *doc, int from_row, int from_col,
                int *out_row, int *out_col);

/*
 * 替换当前匹配（位于 row, col，长度为 match_len）
 * 将 match_len 个字符替换为 replace_str，返回 0；失败返回 -1
 */
int search_replace_current(Document *doc, int row, int col, int match_len);

/*
 * 全部替换：从文档开头扫描并替换所有匹配
 * 返回替换次数
 */
int search_replace_all(Document *doc);
