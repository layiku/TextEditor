/*
 * test_runner.h — 自实现断言宏与全局计数器
 * 零第三方依赖（不使用 Unity/Check 等），仅依赖 C 标准库
 */
#ifndef TEST_RUNNER_H
#define TEST_RUNNER_H

#include <stdio.h>
#include <string.h>

/* 全局通过/失败计数器（定义在 test_runner.c，其他文件 extern 引用） */
extern int g_pass_count;
extern int g_fail_count;

/*
 * ASSERT_EQ(a, b)
 * 检查两个整数值是否相等；失败时打印文件名、行号和两值，并立即返回当前函数
 */
#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        fprintf(stderr, "  FAIL %s:%d  期望 %d，实际 %d\n", \
                __FILE__, __LINE__, (int)(b), (int)(a)); \
        g_fail_count++; return; \
    } else { g_pass_count++; } \
} while(0)

/*
 * ASSERT_NEQ(a, b)
 * 检查两个整数值是否不等
 */
#define ASSERT_NEQ(a, b) do { \
    if ((a) == (b)) { \
        fprintf(stderr, "  FAIL %s:%d  值不应为 %d\n", \
                __FILE__, __LINE__, (int)(a)); \
        g_fail_count++; return; \
    } else { g_pass_count++; } \
} while(0)

/*
 * ASSERT_STR_EQ(a, b)
 * 检查两个字符串是否相等
 */
#define ASSERT_STR_EQ(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        fprintf(stderr, "  FAIL %s:%d  期望 \"%s\"，实际 \"%s\"\n", \
                __FILE__, __LINE__, (b), (a)); \
        g_fail_count++; return; \
    } else { g_pass_count++; } \
} while(0)

/*
 * ASSERT_TRUE(expr)
 * 检查表达式为真
 */
#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "  FAIL %s:%d  表达式为假: %s\n", \
                __FILE__, __LINE__, #expr); \
        g_fail_count++; return; \
    } else { g_pass_count++; } \
} while(0)

/*
 * ASSERT_NULL(ptr) / ASSERT_NOT_NULL(ptr)
 */
#define ASSERT_NULL(ptr) do { \
    if ((ptr) != NULL) { \
        fprintf(stderr, "  FAIL %s:%d  期望 NULL\n", __FILE__, __LINE__); \
        g_fail_count++; return; \
    } else { g_pass_count++; } \
} while(0)

#define ASSERT_NOT_NULL(ptr) do { \
    if ((ptr) == NULL) { \
        fprintf(stderr, "  FAIL %s:%d  不期望 NULL\n", __FILE__, __LINE__); \
        g_fail_count++; return; \
    } else { g_pass_count++; } \
} while(0)

/* 打印测试用例名称（辅助函数） */
#define TEST_CASE(name) \
    printf("  [测试] " name "\n")

#endif /* TEST_RUNNER_H */
