/*
 * test_config.c — config 模块单元测试
 */
#include <stdio.h>
#include <string.h>
#include "test_runner.h"
#include "../include/config.h"

static void test_defaults(void) {
    TEST_CASE("config: 默认值正确");
    /* 不加载任何配置文件，使用内置默认值 */
    ASSERT_EQ(config_get_wrap_mode(), WRAP_CHAR);
    ASSERT_EQ(config_get_show_lineno(), 0);
    ASSERT_EQ(config_get_tab_size(), 4);
    ASSERT_EQ(config_get_max_file_size() > 0, 1);
}

static void test_set_get(void) {
    TEST_CASE("config set/get: wrap_mode 和 show_lineno");
    config_set_wrap_mode(WRAP_NONE);
    ASSERT_EQ(config_get_wrap_mode(), WRAP_NONE);

    config_set_show_lineno(true);
    ASSERT_EQ(config_get_show_lineno(), 1);

    /* 还原 */
    config_set_wrap_mode(WRAP_CHAR);
    config_set_show_lineno(false);
}

static void test_recent(void) {
    TEST_CASE("config recent: 添加与获取最近文件");
    /* 添加 3 个文件 */
    config_add_recent("/path/to/a.txt");
    config_add_recent("/path/to/b.txt");
    config_add_recent("/path/to/c.txt");

    ASSERT_EQ(config_get_recent_count(), 3);
    ASSERT_STR_EQ(config_get_recent(0), "/path/to/c.txt");  /* 最新在前 */
    ASSERT_STR_EQ(config_get_recent(1), "/path/to/b.txt");
    ASSERT_STR_EQ(config_get_recent(2), "/path/to/a.txt");
}

static void test_recent_dedup(void) {
    TEST_CASE("config recent: 重复路径去重并移到最前");
    config_add_recent("/file/x.txt");
    config_add_recent("/file/y.txt");
    config_add_recent("/file/x.txt");  /* 重复，应移到最前 */

    ASSERT_STR_EQ(config_get_recent(0), "/file/x.txt");
}

static void test_load_save(void) {
    TEST_CASE("config load/save: 写文件再读回一致");
    const char *tmpfile = "tests/fixtures/tmp_config.ini";

    config_set_wrap_mode(WRAP_NONE);
    config_set_show_lineno(true);
    config_set_tab_size(2);
    config_save(tmpfile);

    /* 重置为默认值后加载 */
    config_set_wrap_mode(WRAP_CHAR);
    config_set_show_lineno(false);
    config_set_tab_size(4);

    config_load(tmpfile);
    ASSERT_EQ(config_get_wrap_mode(), WRAP_NONE);
    ASSERT_EQ(config_get_show_lineno(), 1);
    ASSERT_EQ(config_get_tab_size(), 2);

    /* 还原 */
    config_set_wrap_mode(WRAP_CHAR);
    config_set_show_lineno(false);
    config_set_tab_size(4);
}

static void test_syntax_override_via_file(void) {
    TEST_CASE("config syntax_ext_*: 从 INI 文件解析扩展名覆盖映射");
    const char *tmpfile = "tests/fixtures/tmp_syntax_ext.ini";

    /* 写一个包含 syntax_ext_* 条目的配置文件 */
    FILE *fp = fopen(tmpfile, "w");
    if (!fp) { fprintf(stderr, "  SKIP: 无法写临时文件\n"); return; }
    fprintf(fp, "syntax_ext_xyz=c.ini\n");
    fprintf(fp, "syntax_ext_myconf=python.ini\n");
    fclose(fp);

    config_load(tmpfile);

    ASSERT_STR_EQ(config_get_syntax_override("xyz"),    "c.ini");
    ASSERT_STR_EQ(config_get_syntax_override("myconf"), "python.ini");
    ASSERT_NULL(config_get_syntax_override("notset"));
    /* 大小写不匹配时应返回 NULL（存储时强制小写） */
    ASSERT_NULL(config_get_syntax_override("XYZ"));
}

static void test_syntax_override_null_empty(void) {
    TEST_CASE("config_get_syntax_override: NULL/空串 → 返回 NULL");
    ASSERT_NULL(config_get_syntax_override(NULL));
    ASSERT_NULL(config_get_syntax_override(""));
}

void test_config_run(void) {
    printf("\n[模块] config\n");
    test_defaults();
    test_set_get();
    test_recent();
    test_recent_dedup();
    test_load_save();
    test_syntax_override_via_file();
    test_syntax_override_null_empty();
}
