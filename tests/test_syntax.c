/*
 * test_syntax.c — syntax 高亮引擎单元测试
 * 直接构造 SyntaxDef（不依赖 INI 文件），验证 highlight_line 的规则匹配逻辑
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#  include <direct.h>   /* _mkdir */
#else
#  include <sys/stat.h> /* mkdir */
#endif
#include "test_runner.h"
#include "../include/types.h"
#include "../include/syntax.h"
#include "../include/document.h"
#include "../include/config.h"

/* 测试用属性值（避免与 0 混淆，用不同颜色确认是引擎写入的） */
#define ATTR_KW   MAKE_ATTR(COLOR_CYAN,    COLOR_BLACK)
#define ATTR_CMT  MAKE_ATTR(COLOR_GREEN,   COLOR_BLACK)
#define ATTR_STR  MAKE_ATTR(COLOR_BROWN,   COLOR_BLACK)  /* DOS 无 YELLOW，用 BROWN 代替 */
#define ATTR_NUM  MAKE_ATTR(COLOR_MAGENTA, COLOR_BLACK)

/* 构造一个只有一条规则的 SyntaxDef */
static SyntaxDef make_def_keyword(const char *word, uint8_t attr) {
    SyntaxDef def;
    memset(&def, 0, sizeof(def));
    def.rule_count = 1;
    def.rules[0].type     = RULE_KEYWORD;
    def.rules[0].priority = 10;
    def.rules[0].attr     = attr;
    def.rules[0].keyword_count = 1;
    strncpy(def.rules[0].keywords[0], word, SYNTAX_KEYWORD_LEN - 1);
    return def;
}

/* ================================================================
 * SyntaxContext 生命周期
 * ================================================================ */

static void test_ctx_init_free(void) {
    TEST_CASE("syntax_ctx_init/free: 安全初始化与释放");
    SyntaxContext ctx;
    syntax_ctx_init(&ctx);
    ASSERT_NULL(ctx.def);
    ASSERT_NULL(ctx.line_in_comment);
    ASSERT_EQ(ctx.cached_count, 0);
    syntax_ctx_free(&ctx);
    ASSERT_NULL(ctx.line_in_comment);
}

static void test_ctx_free_null_def(void) {
    TEST_CASE("syntax_ctx_free: def=NULL 时不崩溃");
    SyntaxContext ctx;
    syntax_ctx_init(&ctx);
    syntax_ctx_free(&ctx);  /* 不应崩溃 */
    ASSERT_NULL(ctx.line_in_comment);
}

/* ================================================================
 * highlight_line — NULL ctx/def 时安全
 * ================================================================ */

static void test_highlight_null_ctx(void) {
    TEST_CASE("syntax_highlight_line: ctx=NULL 时不写入 out_attrs");
    Document *doc = document_new();
    uint8_t attrs[8] = {1, 1, 1, 1, 1, 1, 1, 1};  /* 预填非零 */
    syntax_highlight_line(NULL, doc, 0, attrs, 8);
    /* NULL ctx → 函数立即返回，attrs 不应被清零 */
    ASSERT_EQ(attrs[0], 1);
    document_free(doc);
}

static void test_highlight_null_def(void) {
    TEST_CASE("syntax_highlight_line: def=NULL 时不写入 out_attrs");
    SyntaxContext ctx;
    syntax_ctx_init(&ctx);  /* def = NULL */
    Document *doc = document_new();
    uint8_t attrs[8] = {1, 1, 1, 1, 1, 1, 1, 1};
    syntax_highlight_line(&ctx, doc, 0, attrs, 8);
    ASSERT_EQ(attrs[0], 1);  /* 不应被改动 */
    document_free(doc);
    syntax_ctx_free(&ctx);
}

/* ================================================================
 * 关键词规则
 * ================================================================ */

static void test_keyword_highlight(void) {
    TEST_CASE("syntax: 关键词 'int' 正确高亮");
    SyntaxDef def = make_def_keyword("int", ATTR_KW);

    SyntaxContext ctx;
    syntax_ctx_init(&ctx);
    ctx.def = &def;

    Document *doc = document_new();
    /* 写入 "int x;" 到第 0 行 */
    int or_, oc;
    document_insert_text(doc, 0, 0, "int x;", 6, &or_, &oc);

    syntax_rebuild_state(&ctx, doc, -1);

    uint8_t attrs[8] = {0};
    syntax_highlight_line(&ctx, doc, 0, attrs, 8);

    /* 字节 0-2 应为 ATTR_KW，其余为 0 */
    ASSERT_EQ(attrs[0], ATTR_KW);
    ASSERT_EQ(attrs[1], ATTR_KW);
    ASSERT_EQ(attrs[2], ATTR_KW);
    ASSERT_EQ(attrs[3], 0);
    ASSERT_EQ(attrs[4], 0);
    ASSERT_EQ(attrs[5], 0);

    document_free(doc);
    syntax_ctx_free(&ctx);
    /* def 是栈上对象，无需 syntax_free */
}

static void test_keyword_no_partial_match(void) {
    TEST_CASE("syntax: 关键词不匹配更长的标识符 'integer'");
    SyntaxDef def = make_def_keyword("int", ATTR_KW);

    SyntaxContext ctx;
    syntax_ctx_init(&ctx);
    ctx.def = &def;

    Document *doc = document_new();
    int or_, oc;
    document_insert_text(doc, 0, 0, "integer", 7, &or_, &oc);

    syntax_rebuild_state(&ctx, doc, -1);
    uint8_t attrs[8] = {0};
    syntax_highlight_line(&ctx, doc, 0, attrs, 8);

    /* "int" 在 "integer" 内，不应匹配（词边界检查） */
    ASSERT_EQ(attrs[0], 0);

    document_free(doc);
    syntax_ctx_free(&ctx);
}

static void test_keyword_at_end(void) {
    TEST_CASE("syntax: 行尾关键词高亮");
    SyntaxDef def = make_def_keyword("return", ATTR_KW);

    SyntaxContext ctx;
    syntax_ctx_init(&ctx);
    ctx.def = &def;

    Document *doc = document_new();
    int or_, oc;
    document_insert_text(doc, 0, 0, "x = return", 10, &or_, &oc);

    syntax_rebuild_state(&ctx, doc, -1);
    uint8_t attrs[16] = {0};
    syntax_highlight_line(&ctx, doc, 0, attrs, 16);

    /* "return" 从位置 4 开始 */
    ASSERT_EQ(attrs[4], ATTR_KW);
    ASSERT_EQ(attrs[9], ATTR_KW);
    ASSERT_EQ(attrs[3], 0);

    document_free(doc);
    syntax_ctx_free(&ctx);
}

/* ================================================================
 * 行注释规则
 * ================================================================ */

static void test_line_comment(void) {
    TEST_CASE("syntax: 行注释 '//' 到行尾高亮");
    SyntaxDef def;
    memset(&def, 0, sizeof(def));
    def.rule_count = 1;
    def.rules[0].type     = RULE_LINE_COMMENT;
    def.rules[0].priority = 20;
    def.rules[0].attr     = ATTR_CMT;
    strncpy(def.rules[0].prefix, "//", sizeof(def.rules[0].prefix) - 1);

    SyntaxContext ctx;
    syntax_ctx_init(&ctx);
    ctx.def = &def;

    Document *doc = document_new();
    int or_, oc;
    document_insert_text(doc, 0, 0, "x = 1; // comment", 17, &or_, &oc);

    syntax_rebuild_state(&ctx, doc, -1);
    uint8_t attrs[20] = {0};
    syntax_highlight_line(&ctx, doc, 0, attrs, 20);

    /* "//" 从字节 7 开始 */
    ASSERT_EQ(attrs[0], 0);   /* 非注释 */
    ASSERT_EQ(attrs[7], ATTR_CMT);
    ASSERT_EQ(attrs[16], ATTR_CMT);

    document_free(doc);
    syntax_ctx_free(&ctx);
}

/* ================================================================
 * 字符串规则
 * ================================================================ */

static void test_string_highlight(void) {
    TEST_CASE("syntax: 双引号字符串高亮");
    SyntaxDef def;
    memset(&def, 0, sizeof(def));
    def.rule_count = 1;
    def.rules[0].type      = RULE_STRING;
    def.rules[0].priority  = 15;
    def.rules[0].attr      = ATTR_STR;
    def.rules[0].delimiter = '"';
    def.rules[0].escape    = '\\';

    SyntaxContext ctx;
    syntax_ctx_init(&ctx);
    ctx.def = &def;

    Document *doc = document_new();
    int or_, oc;
    /* x = "hi"; → 字节 4-7 = "hi" */
    document_insert_text(doc, 0, 0, "x = \"hi\";", 9, &or_, &oc);

    syntax_rebuild_state(&ctx, doc, -1);
    uint8_t attrs[12] = {0};
    syntax_highlight_line(&ctx, doc, 0, attrs, 12);

    ASSERT_EQ(attrs[0], 0);   /* x 不高亮 */
    ASSERT_EQ(attrs[4], ATTR_STR);  /* 开头 '"' */
    ASSERT_EQ(attrs[7], ATTR_STR);  /* 结尾 '"' */
    ASSERT_EQ(attrs[8], 0);         /* ';' 不高亮 */

    document_free(doc);
    syntax_ctx_free(&ctx);
}

/* ================================================================
 * 数字规则
 * ================================================================ */

static void test_number_highlight(void) {
    TEST_CASE("syntax: 整数数字字面量高亮");
    SyntaxDef def;
    memset(&def, 0, sizeof(def));
    def.rule_count = 1;
    def.rules[0].type     = RULE_NUMBER;
    def.rules[0].priority = 10;
    def.rules[0].attr     = ATTR_NUM;

    SyntaxContext ctx;
    syntax_ctx_init(&ctx);
    ctx.def = &def;

    Document *doc = document_new();
    int or_, oc;
    /* "x = 42;" → "42" 在字节 4-5 */
    document_insert_text(doc, 0, 0, "x = 42;", 7, &or_, &oc);

    syntax_rebuild_state(&ctx, doc, -1);
    uint8_t attrs[8] = {0};
    syntax_highlight_line(&ctx, doc, 0, attrs, 8);

    ASSERT_EQ(attrs[4], ATTR_NUM);
    ASSERT_EQ(attrs[5], ATTR_NUM);
    ASSERT_EQ(attrs[3], 0);   /* 空格不高亮 */
    ASSERT_EQ(attrs[6], 0);   /* ';' 不高亮 */

    document_free(doc);
    syntax_ctx_free(&ctx);
}

static void test_number_hex(void) {
    TEST_CASE("syntax: 十六进制数字 0xFF 高亮");
    SyntaxDef def;
    memset(&def, 0, sizeof(def));
    def.rule_count = 1;
    def.rules[0].type     = RULE_NUMBER;
    def.rules[0].priority = 10;
    def.rules[0].attr     = ATTR_NUM;

    SyntaxContext ctx;
    syntax_ctx_init(&ctx);
    ctx.def = &def;

    Document *doc = document_new();
    int or_, oc;
    document_insert_text(doc, 0, 0, "0xFF", 4, &or_, &oc);

    syntax_rebuild_state(&ctx, doc, -1);
    uint8_t attrs[6] = {0};
    syntax_highlight_line(&ctx, doc, 0, attrs, 6);

    ASSERT_EQ(attrs[0], ATTR_NUM);
    ASSERT_EQ(attrs[3], ATTR_NUM);

    document_free(doc);
    syntax_ctx_free(&ctx);
}

/* ================================================================
 * 多规则优先级
 * ================================================================ */

static void test_priority_comment_over_keyword(void) {
    TEST_CASE("syntax: 注释内的关键词不被二次高亮（注释优先级更高）");
    SyntaxDef def;
    memset(&def, 0, sizeof(def));
    def.rule_count = 2;

    /* 规则 0：行注释（优先级 20） */
    def.rules[0].type     = RULE_LINE_COMMENT;
    def.rules[0].priority = 20;
    def.rules[0].attr     = ATTR_CMT;
    strncpy(def.rules[0].prefix, "//", sizeof(def.rules[0].prefix) - 1);

    /* 规则 1：关键词（优先级 10） */
    def.rules[1].type     = RULE_KEYWORD;
    def.rules[1].priority = 10;
    def.rules[1].attr     = ATTR_KW;
    def.rules[1].keyword_count = 1;
    strncpy(def.rules[1].keywords[0], "int", SYNTAX_KEYWORD_LEN - 1);

    SyntaxContext ctx;
    syntax_ctx_init(&ctx);
    ctx.def = &def;

    Document *doc = document_new();
    int or_, oc;
    /* "// int x" — "int" 在注释里 */
    document_insert_text(doc, 0, 0, "// int x", 8, &or_, &oc);

    syntax_rebuild_state(&ctx, doc, -1);
    uint8_t attrs[10] = {0};
    syntax_highlight_line(&ctx, doc, 0, attrs, 10);

    /* 整行应为注释颜色（优先级 20 > 10） */
    ASSERT_EQ(attrs[0], ATTR_CMT);
    ASSERT_EQ(attrs[3], ATTR_CMT);  /* 'i' in "int" 应是注释色 */

    document_free(doc);
    syntax_ctx_free(&ctx);
}

/* ================================================================
 * syntax_list_languages
 * ================================================================ */

static void test_list_languages_nonexistent(void) {
    TEST_CASE("syntax_list_languages: 目录不存在 → 返回 0，不崩溃");
    SyntaxLangInfo out[8];
    int n = syntax_list_languages("nonexistent_dir_xyz_12345", out, 8);
    ASSERT_EQ(n, 0);
}

static void test_list_languages_zero_cap(void) {
    TEST_CASE("syntax_list_languages: max_count=0 → 返回 0，不越界");
    int n = syntax_list_languages("syntax", NULL, 0);
    ASSERT_EQ(n, 0);
}

/* ================================================================
 * syntax_match_ext：未知扩展名 → 返回 NULL
 * ================================================================ */

static void test_match_ext_unknown(void) {
    TEST_CASE("syntax_match_ext: 未知扩展名无覆盖 → 返回 NULL");
    /* "zzz" 不在任何 INI 的 extensions 中，也无 config 覆盖 */
    SyntaxDef *def = syntax_match_ext("file.zzz_unknown_ext", NULL);
    ASSERT_NULL(def);
}

static void test_match_ext_null(void) {
    TEST_CASE("syntax_match_ext: NULL/无扩展名路径 → 返回 NULL");
    ASSERT_NULL(syntax_match_ext(NULL, NULL));
    ASSERT_NULL(syntax_match_ext("", NULL));
    ASSERT_NULL(syntax_match_ext("noextension", NULL));
}

/* ================================================================
 * syntax_match_ext 使用 config 覆盖映射
 * 写一个最小的 INI 文件到 tests/fixtures/，通过 exe_dir 让引擎找到它，
 * 再配置 syntax_ext_tst=tst_syntax_test.ini，验证 def 被正确加载。
 * ================================================================ */

static void test_match_ext_config_override(void) {
    TEST_CASE("syntax_match_ext: config 覆盖映射优先于内置别名");

    /* 在 tests/fixtures/syntax/ 下写一个最小 INI */
    const char *ini_dir  = "tests/fixtures/syntax";
    const char *ini_path = "tests/fixtures/syntax/tst_syntax_test.ini";

#ifdef _WIN32
    _mkdir(ini_dir);
#else
    mkdir(ini_dir, 0755);
#endif

    FILE *fp = fopen(ini_path, "w");
    if (!fp) { fprintf(stderr, "  SKIP: 无法创建测试 INI\n"); return; }
    fprintf(fp, "[meta]\nname=TestLang\nextensions=.tst\n");
    fprintf(fp, "[rule.kw]\ntype=keyword\ncolor=cyan\npriority=10\nwords=hello\n");
    fclose(fp);

    /* 配置 syntax_ext_tst → tst_syntax_test.ini */
    const char *cfg_path = "tests/fixtures/tmp_override_cfg.ini";
    fp = fopen(cfg_path, "w");
    if (!fp) { fprintf(stderr, "  SKIP: 无法创建配置文件\n"); return; }
    fprintf(fp, "syntax_ext_tst=tst_syntax_test.ini\n");
    fclose(fp);

    config_load(cfg_path);

    /* exe_dir 指向 tests/fixtures，使引擎在 tests/fixtures/syntax/ 下查找 */
    SyntaxDef *def = syntax_match_ext("file.tst", "tests/fixtures");
    ASSERT_NOT_NULL(def);
    if (def) {
        ASSERT_STR_EQ(def->name, "TestLang");
        syntax_free(def);
    }
}

/* ================================================================
 * 入口
 * ================================================================ */

void test_syntax_run(void) {
    printf("\n[test_syntax]\n");

    test_ctx_init_free();
    test_ctx_free_null_def();
    test_highlight_null_ctx();
    test_highlight_null_def();
    test_keyword_highlight();
    test_keyword_no_partial_match();
    test_keyword_at_end();
    test_line_comment();
    test_string_highlight();
    test_number_highlight();
    test_number_hex();
    test_priority_comment_over_keyword();
    test_list_languages_nonexistent();
    test_list_languages_zero_cap();
    test_match_ext_unknown();
    test_match_ext_null();
    test_match_ext_config_override();
}
