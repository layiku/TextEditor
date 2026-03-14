/*
 * clipboard.h — 内部剪贴板接口
 * 全程内存存储，支持多行（含 \n），不依赖系统剪贴板（DOS 兼容）
 */
#pragma once

/* 初始化（程序启动时调用一次） */
void clipboard_init(void);

/* 释放资源（程序退出时调用） */
void clipboard_exit(void);

/* 将 len 字节的 text 写入剪贴板（会复制内容，text 可含 \n） */
void clipboard_set(const char *text, int len);

/* 获取剪贴板内容，*out_len 设为字节数；返回内部指针（只读，不可 free） */
const char* clipboard_get(int *out_len);

/* 检查剪贴板是否为空 */
int clipboard_is_empty(void);
