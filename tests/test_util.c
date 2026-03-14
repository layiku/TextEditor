/*
 * test_util.c — util 模块单元测试
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "test_runner.h"
#include "../include/util.h"

static void test_trim_crlf(void) {
    TEST_CASE("str_trim_crlf: 去除行尾 \\r\\n");
    char s1[] = "hello\r\n";
    str_trim_crlf(s1);
    ASSERT_STR_EQ(s1, "hello");

    char s2[] = "world\n";
    str_trim_crlf(s2);
    ASSERT_STR_EQ(s2, "world");

    char s3[] = "noend";
    str_trim_crlf(s3);
    ASSERT_STR_EQ(s3, "noend");
}

static void test_str_icmp(void) {
    TEST_CASE("str_icmp: 大小写无关比较");
    ASSERT_EQ(str_icmp("Hello", "hello"), 0);
    ASSERT_EQ(str_icmp("ABC", "ABD") < 0, 1);
    ASSERT_EQ(str_icmp("xyz", "XYZ"), 0);
}

static void test_str_istr(void) {
    TEST_CASE("str_istr: 大小写无关子串查找");
    /* 必须用同一个变量保存基址，
     * MSVC 不保证同一字面量出现两次时地址相同，指针相减会得到错误偏移 */
    const char *base = "Hello World";
    const char *r = str_istr(base, "world");
    ASSERT_NOT_NULL(r);
    ASSERT_EQ((int)(r - base), 6);

    ASSERT_NULL(str_istr("foo bar", "baz"));
}

static void test_starts_with(void) {
    TEST_CASE("str_starts_with: 前缀匹配");
    ASSERT_EQ(str_starts_with("foobar", "foo"), 1);
    ASSERT_EQ(str_starts_with("foobar", "bar"), 0);
    ASSERT_EQ(str_starts_with("foo", "foo"), 1);
}

static void test_safe_malloc(void) {
    TEST_CASE("safe_malloc/safe_strdup: 基本分配");
    char *p = (char*)safe_malloc(10);
    ASSERT_NOT_NULL(p);
    free(p);

    char *s = safe_strdup("test");
    ASSERT_STR_EQ(s, "test");
    free(s);
}

static void test_path_get_dir(void) {
    TEST_CASE("path_get_dir: 提取目录部分");
    char out[128];
    path_get_dir("/home/user/file.txt", out, sizeof(out));
    ASSERT_STR_EQ(out, "/home/user");

    path_get_dir("file.txt", out, sizeof(out));
    ASSERT_STR_EQ(out, ".");
}

static void test_path_join(void) {
    TEST_CASE("path_join: 拼接路径");
    char out[128];
    path_join("/home/user", "file.txt", out, sizeof(out));
    ASSERT_STR_EQ(out, "/home/user/file.txt");

    path_join("/home/user/", "file.txt", out, sizeof(out));
    ASSERT_STR_EQ(out, "/home/user/file.txt");
}

static void test_clamp(void) {
    TEST_CASE("CLAMP: 边界值夹取");
    ASSERT_EQ(CLAMP(5, 0, 10), 5);
    ASSERT_EQ(CLAMP(-1, 0, 10), 0);
    ASSERT_EQ(CLAMP(15, 0, 10), 10);
}

static void test_path_is_dir(void) {
    TEST_CASE("path_is_dir: 目录返回 true，文件返回 false，不存在返回 false");
    /* tests/ 目录存在 */
    ASSERT_TRUE(path_is_dir("tests"));
    /* 已知普通文件 */
    ASSERT_EQ(path_is_dir("tests/test_runner.c"), 0);
    /* 不存在的路径 */
    ASSERT_EQ(path_is_dir("nonexistent_xyz_99"), 0);
}

static void test_dir_read_entries_basic(void) {
    TEST_CASE("dir_read_entries: 读取 tests/fixtures/ 包含文件和 \"..\" 条目");
    DirEntry entries[32];
    int n = dir_read_entries("tests/fixtures", entries, 32);
    /* fixtures/ 至少有 tmp_config.ini、tmp_test.txt 两个文件加上 ".." */
    ASSERT_TRUE(n >= 3);
    /* 第一条目必须是 ".."（排序规则：".." 置首） */
    ASSERT_STR_EQ(entries[0].name, "..");
    ASSERT_EQ(entries[0].is_dir, 1);
}

static void test_dir_read_entries_files_after_dirs(void) {
    TEST_CASE("dir_read_entries: 目录排在文件之前（除 \"..\" 外）");
    DirEntry entries[64];
    int n = dir_read_entries("tests", entries, 64);
    ASSERT_TRUE(n >= 2);
    /* 跳过 ".." 后，第一个非 ".." 条目若是目录则必须排在普通文件前 */
    bool saw_file = false;
    for (int i = 1; i < n; i++) {
        if (!entries[i].is_dir) {
            saw_file = true;
        } else {
            /* 目录不应出现在文件之后 */
            ASSERT_EQ(saw_file, 0);
        }
    }
    (void)saw_file;
    ASSERT_EQ(1, 1);  /* 排序结构验证通过 */
}

static void test_dir_read_entries_max_count(void) {
    TEST_CASE("dir_read_entries: max_count=0 → 返回 0，不越界");
    int n = dir_read_entries("tests", NULL, 0);
    ASSERT_EQ(n, 0);
}

static void test_dir_read_entries_nonexistent(void) {
    TEST_CASE("dir_read_entries: 目录不存在 → 返回 0，不崩溃");
    DirEntry entries[8];
    int n = dir_read_entries("nonexistent_dir_xyz_9999", entries, 8);
    ASSERT_EQ(n, 0);
}

void test_util_run(void) {
    printf("\n[模块] util\n");
    test_trim_crlf();
    test_str_icmp();
    test_str_istr();
    test_starts_with();
    test_safe_malloc();
    test_path_get_dir();
    test_path_join();
    test_clamp();
    test_path_is_dir();
    test_dir_read_entries_basic();
    test_dir_read_entries_files_after_dirs();
    test_dir_read_entries_max_count();
    test_dir_read_entries_nonexistent();
}
