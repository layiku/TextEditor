/*
 * test_runner.c — 测试框架入口
 * 定义断言宏、维护通过/失败计数、调用各模块测试函数
 * 零第三方依赖，仅使用 C 标准库
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_runner.h"

/* 全局计数器（各测试文件通过 extern 访问） */
int g_pass_count = 0;
int g_fail_count = 0;

/* 各测试模块入口声明 */
void test_document_run(void);
void test_clipboard_run(void);
void test_search_run(void);
void test_viewport_run(void);
void test_util_run(void);
void test_config_run(void);
void test_editor_run(void);
void test_utf8_run(void);
void test_encoding_run(void);  /* Phase 6B：编码检测与转换 */
void test_syntax_run(void);    /* Phase 6C：语法高亮引擎   */

int main(void) {
    printf("=== 开始自动测试 ===\n\n");

    test_document_run();
    test_clipboard_run();
    test_search_run();
    test_viewport_run();
    test_util_run();
    test_config_run();
    test_editor_run();
    test_utf8_run();
    test_encoding_run();
    test_syntax_run();

    printf("\n=== 测试结果：%d 通过，%d 失败 ===\n",
           g_pass_count, g_fail_count);
    return g_fail_count > 0 ? 1 : 0;
}
