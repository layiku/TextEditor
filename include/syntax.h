/*
 * syntax.h — 语法高亮引擎接口
 *
 * 设计目标：
 *   - 规则从外部 INI 文件加载（syntax/<lang>.ini），可用户自定义
 *   - 按扩展名自动匹配语言
 *   - 支持关键词、字符串、行注释、块注释（跨行状态跟踪）、数字、行首前缀、正则
 *   - 高亮属性数组 (uint8_t[]) 按字节索引映射到文档行，与 viewport 解耦
 *   - 零第三方依赖（正则由 regex_simple 提供）
 *
 * INI 配置文件格式示例见 syntax/c.ini
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "document.h"

/* ================================================================
 * 规则类型
 * ================================================================ */
typedef enum {
    RULE_KEYWORD      = 0,  /* 关键词列表（词边界匹配） */
    RULE_STRING       = 1,  /* 字符串字面量（定界符对） */
    RULE_LINE_COMMENT = 2,  /* 行注释（到行尾） */
    RULE_BLOCK_COMMENT= 3,  /* 块注释（跨行，slash-star ... star-slash 形式） */
    RULE_NUMBER       = 4,  /* 数字字面量（内建模式） */
    RULE_LINE_START   = 5,  /* 行首前缀（如 C 预处理 #include） */
    RULE_REGEX        = 6,  /* 自定义正则模式 */
} SyntaxRuleType;

/* 关键词表最大条数 */
#define SYNTAX_MAX_KEYWORDS 128
/* 单个关键词最大长度 */
#define SYNTAX_KEYWORD_LEN  32

/* ================================================================
 * 单条规则
 * ================================================================ */
typedef struct {
    SyntaxRuleType type;
    int            priority;   /* 数值越高优先级越高，相同位置优先取高 */
    uint8_t        attr;       /* 颜色属性（MAKE_ATTR 构造） */

    /* RULE_KEYWORD */
    char keywords[SYNTAX_MAX_KEYWORDS][SYNTAX_KEYWORD_LEN];
    int  keyword_count;

    /* RULE_STRING */
    char delimiter;   /* 起始/结束定界符，如 '"' 或 '\'' */
    char escape;      /* 转义字符，如 '\\'，0=无转义 */

    /* RULE_LINE_COMMENT / RULE_LINE_START */
    char prefix[16];

    /* RULE_BLOCK_COMMENT */
    char block_start[16];
    char block_end[16];

    /* RULE_REGEX */
    char pattern[128];
} SyntaxRule;

/* ================================================================
 * 语法定义（从一个 INI 文件加载）
 * ================================================================ */
#define SYNTAX_MAX_RULES 32

typedef struct {
    char       name[64];          /* [meta] name */
    char       extensions[256];   /* [meta] extensions，空格分隔 */
    SyntaxRule rules[SYNTAX_MAX_RULES];
    int        rule_count;
} SyntaxDef;

/* ================================================================
 * 每文档语法状态（块注释跨行跟踪）
 * ================================================================ */
typedef struct {
    SyntaxDef *def;             /* NULL = 不高亮 */
    bool      *line_in_comment; /* line_in_comment[i]=true 表示第 i 行开始时处于块注释内 */
    int        cached_count;    /* line_in_comment 已分配的行数 */
    bool       dirty;           /* 文档改动后需要重建 */
} SyntaxContext;

/* ================================================================
 * 接口
 * ================================================================ */

/* 从 INI 文件加载语法定义，失败返回 NULL */
SyntaxDef* syntax_load(const char *ini_path);

/* 释放 syntax_load 返回的对象 */
void       syntax_free(SyntaxDef *def);

/* 按文件路径扩展名在 syntax/ 目录下自动查找匹配的 .ini 文件并加载。
 * exe_dir — 可执行文件所在目录（可为 NULL，仅查找当前目录）
 * 返回 NULL 表示不支持该语言 */
SyntaxDef* syntax_match_ext(const char *filepath, const char *exe_dir);

/* 重建块注释行状态缓存（文档内容变化后调用）。
 * 增量：从 changed_row 开始扫描（-1 = 全量） */
void syntax_rebuild_state(SyntaxContext *ctx, const Document *doc, int changed_row);

/* 对文档第 doc_row 行计算语法高亮属性。
 * out_attrs[i] = 0 表示无特殊高亮（使用 ATTR_NORMAL）；否则为 MAKE_ATTR(fg,bg)。
 * attrs_len 为 out_attrs 数组容量。 */
void syntax_highlight_line(const SyntaxContext *ctx, const Document *doc,
                            int doc_row, uint8_t *out_attrs, int attrs_len);

/* 初始化 / 销毁 SyntaxContext（生命周期跟随 Editor） */
void syntax_ctx_init(SyntaxContext *ctx);
void syntax_ctx_free(SyntaxContext *ctx);

/* ================================================================
 * 语言列表扫描（Phase 7：手动切换语法高亮）
 * ================================================================ */

/* 单个语言条目 */
#define SYNTAX_MAX_LANGS 32

typedef struct {
    char name[64];   /* [meta] name 字段，如 "C/C++ Source" */
    char path[512];  /* .ini 文件完整路径 */
} SyntaxLangInfo;

/* 扫描 syntax_dir 目录下所有 .ini 文件，读取各文件的 [meta] name。
 * 返回找到的语言数量（0..max_count）。
 * syntax_dir 不存在或为空时安全返回 0。 */
int syntax_list_languages(const char *syntax_dir,
                           SyntaxLangInfo *out, int max_count);
