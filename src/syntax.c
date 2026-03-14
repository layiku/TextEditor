/*
 * syntax.c — 语法高亮引擎实现
 *
 * 规则优先级：priority 值越大，优先级越高。
 * 同一字节位置可能被多条规则覆盖，只保留优先级最高的属性。
 * 块注释规则（RULE_BLOCK_COMMENT）在跨行状态中最优先。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "syntax.h"
#include "regex_simple.h"
#include "util.h"
#include "config.h"

/* ================================================================
 * 内部：颜色名称 → 属性字节
 * ================================================================ */

/* 解析 color 字符串 → uint8_t 属性（黑背景） */
static uint8_t parse_color(const char *s) {
    /* 支持 bright_ 前缀 */
    int bright = 0;
    if (strncmp(s, "bright_", 7) == 0) { bright = 1; s += 7; }

    uint8_t fg;
    if      (strcmp(s, "black")   == 0) fg = 0;
    else if (strcmp(s, "blue")    == 0) fg = 1;
    else if (strcmp(s, "green")   == 0) fg = 2;
    else if (strcmp(s, "cyan")    == 0) fg = 3;
    else if (strcmp(s, "red")     == 0) fg = 4;
    else if (strcmp(s, "magenta") == 0) fg = 5;
    else if (strcmp(s, "yellow")  == 0) fg = 6;  /* 映射到 brown/yellow */
    else if (strcmp(s, "brown")   == 0) fg = 6;
    else if (strcmp(s, "white")   == 0) fg = 7;
    else if (strcmp(s, "grey")    == 0) fg = 8;  /* dark_grey = bright black */
    else if (strcmp(s, "dark_grey")== 0) fg = 8;
    else fg = 7;  /* 默认白色 */

    if (bright) fg |= 8;
    /* 背景始终为黑 */
    return (uint8_t)fg;  /* MAKE_ATTR(fg, 0) = fg */
}

/* ================================================================
 * INI 加载
 * ================================================================ */

/* 去除字符串两端空白 */
static void trim_spaces(char *s) {
    /* 尾部 */
    int len = (int)strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' ||
                       s[len-1] == '\r' || s[len-1] == '\n'))
        s[--len] = '\0';
    /* 头部（通过偏移指针后 memmove） */
    int start = 0;
    while (s[start] == ' ' || s[start] == '\t') start++;
    if (start > 0) memmove(s, s + start, (size_t)(len - start + 1));
}

/* 将空格分隔的 words 字符串解析到 rule->keywords[] */
static void parse_keywords(SyntaxRule *rule, const char *words) {
    char buf[2048];
    strncpy(buf, words, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *tok = strtok(buf, " \t");
    while (tok && rule->keyword_count < SYNTAX_MAX_KEYWORDS) {
        strncpy(rule->keywords[rule->keyword_count], tok, SYNTAX_KEYWORD_LEN - 1);
        rule->keyword_count++;
        tok = strtok(NULL, " \t");
    }
}

SyntaxDef* syntax_load(const char *ini_path) {
    FILE *fp = fopen(ini_path, "r");
    if (!fp) return NULL;

    SyntaxDef *def = (SyntaxDef*)calloc(1, sizeof(SyntaxDef));
    if (!def) { fclose(fp); return NULL; }

    /* 当前正在填充的规则（-1 = 还没进入 rule.xxx 区段） */
    int cur_rule = -1;
    char section[64] = {0};

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        str_trim_crlf(line);
        trim_spaces(line);
        if (!line[0] || line[0] == ';' || line[0] == '#') continue;

        /* ---- 区段头 [xxx] ---- */
        if (line[0] == '[') {
            char *end = strchr(line, ']');
            if (!end) continue;
            *end = '\0';
            strncpy(section, line + 1, sizeof(section) - 1);
            trim_spaces(section);

            /* 判断是否 rule.xxx 区段 */
            if (strncmp(section, "rule.", 5) == 0) {
                if (def->rule_count < SYNTAX_MAX_RULES) {
                    cur_rule = def->rule_count++;
                    /* 默认优先级和属性 */
                    def->rules[cur_rule].priority = 1;
                    def->rules[cur_rule].attr     = 7; /* white */
                } else {
                    cur_rule = -1;  /* 超出上限，忽略 */
                }
            } else {
                cur_rule = -1;  /* [meta] 或其他区段 */
            }
            continue;
        }

        /* ---- key = value ---- */
        char *eq = strchr(line, '=');
        if (!eq) continue;

        char key[64]  = {0};
        char val[512] = {0};
        int klen = (int)(eq - line);
        if (klen <= 0 || klen >= (int)sizeof(key)) continue;
        strncpy(key, line, (size_t)klen);
        trim_spaces(key);
        strncpy(val, eq + 1, sizeof(val) - 1);
        trim_spaces(val);
        if (!val[0]) continue;

        /* ---- [meta] 字段 ---- */
        if (strcmp(section, "meta") == 0) {
            if      (strcmp(key, "name")       == 0) strncpy(def->name, val, sizeof(def->name)-1);
            else if (strcmp(key, "extensions") == 0) strncpy(def->extensions, val, sizeof(def->extensions)-1);
            continue;
        }

        /* ---- rule.xxx 字段 ---- */
        if (cur_rule < 0) continue;
        SyntaxRule *r = &def->rules[cur_rule];

        if (strcmp(key, "type") == 0) {
            if      (strcmp(val, "keyword")       == 0) r->type = RULE_KEYWORD;
            else if (strcmp(val, "string")        == 0) r->type = RULE_STRING;
            else if (strcmp(val, "line_comment")  == 0) r->type = RULE_LINE_COMMENT;
            else if (strcmp(val, "block_comment") == 0) r->type = RULE_BLOCK_COMMENT;
            else if (strcmp(val, "number")        == 0) r->type = RULE_NUMBER;
            else if (strcmp(val, "line_start")    == 0) r->type = RULE_LINE_START;
            else if (strcmp(val, "regex")         == 0) r->type = RULE_REGEX;
        } else if (strcmp(key, "color") == 0) {
            r->attr = parse_color(val);
        } else if (strcmp(key, "priority") == 0) {
            r->priority = atoi(val);
        } else if (strcmp(key, "words") == 0) {
            /* 支持行尾 \ 续行 */
            char words[2048] = {0};
            strncpy(words, val, sizeof(words) - 1);
            /* 检查续行 */
            while (words[strlen(words) - 1] == '\\') {
                words[strlen(words) - 1] = ' ';
                if (!fgets(line, sizeof(line), fp)) break;
                str_trim_crlf(line);
                trim_spaces(line);
                strncat(words, line, sizeof(words) - strlen(words) - 1);
            }
            parse_keywords(r, words);
        } else if (strcmp(key, "delimiter") == 0) {
            r->delimiter = val[0];
        } else if (strcmp(key, "escape") == 0) {
            r->escape = val[0];
        } else if (strcmp(key, "prefix") == 0) {
            strncpy(r->prefix, val, sizeof(r->prefix) - 1);
        } else if (strcmp(key, "start") == 0) {
            strncpy(r->block_start, val, sizeof(r->block_start) - 1);
        } else if (strcmp(key, "end") == 0) {
            strncpy(r->block_end, val, sizeof(r->block_end) - 1);
        } else if (strcmp(key, "pattern") == 0) {
            strncpy(r->pattern, val, sizeof(r->pattern) - 1);
        }
    }

    fclose(fp);
    return def;
}

void syntax_free(SyntaxDef *def) {
    free(def);
}

/* ================================================================
 * 按扩展名查找语法文件
 * 搜索路径（同 config.ini 逻辑）：
 *   exe_dir/syntax/<ext>.ini（如 syntax/c.ini）
 *   ./syntax/<ext>.ini
 * ================================================================ */
SyntaxDef* syntax_match_ext(const char *filepath, const char *exe_dir) {
    if (!filepath || !filepath[0]) return NULL;

    /* 提取扩展名（含点），如 ".c" */
    const char *dot = NULL;
    const char *p   = filepath;
    while (*p) { if (*p == '.') dot = p; p++; }
    if (!dot) return NULL;

    /* 去掉点，用小写扩展名作为文件名 */
    char ext[32] = {0};
    strncpy(ext, dot + 1, sizeof(ext) - 1);
    for (int i = 0; ext[i]; i++) ext[i] = (char)tolower((unsigned char)ext[i]);

    char ini_path[512];
    SyntaxDef *def = NULL;

    /* 优先：config.ini 中的 syntax_ext_<ext>=<ini_name> 覆盖 */
    const char *override = config_get_syntax_override(ext);
    if (override && override[0]) {
        if (exe_dir && exe_dir[0]) {
            snprintf(ini_path, sizeof(ini_path), "%s/syntax/%s", exe_dir, override);
            def = syntax_load(ini_path);
            if (def) return def;
        }
        snprintf(ini_path, sizeof(ini_path), "syntax/%s", override);
        def = syntax_load(ini_path);
        if (def) return def;
    }

    /* 内置别名：.cpp/.hpp/.cc → c，.py → python，etc. */
    const char *lang = ext;
    if (strcmp(ext, "cpp") == 0 || strcmp(ext, "hpp") == 0 ||
        strcmp(ext, "cc")  == 0 || strcmp(ext, "hh")  == 0 ||
        strcmp(ext, "cxx") == 0)
        lang = "c";
    else if (strcmp(ext, "py") == 0 || strcmp(ext, "pyw") == 0)
        lang = "python";
    else if (strcmp(ext, "md") == 0 || strcmp(ext, "markdown") == 0)
        lang = "markdown";
    else if (strcmp(ext, "js") == 0 || strcmp(ext, "ts") == 0)
        lang = "javascript";
    else if (strcmp(ext, "sh") == 0 || strcmp(ext, "bash") == 0)
        lang = "shell";

    /* 1. exe_dir/syntax/<lang>.ini */
    if (exe_dir && exe_dir[0]) {
        snprintf(ini_path, sizeof(ini_path), "%s/syntax/%s.ini", exe_dir, lang);
        def = syntax_load(ini_path);
        if (def) return def;
    }

    /* 2. ./syntax/<lang>.ini（当前目录） */
    snprintf(ini_path, sizeof(ini_path), "syntax/%s.ini", lang);
    def = syntax_load(ini_path);
    return def;
}

/* ================================================================
 * 块注释状态跟踪
 * line_in_comment[i] = true 表示第 i 行"进入"时处于块注释内
 * ================================================================ */

/* 找到第一条 BLOCK_COMMENT 规则 */
static const SyntaxRule* find_block_comment_rule(const SyntaxDef *def) {
    for (int i = 0; i < def->rule_count; i++)
        if (def->rules[i].type == RULE_BLOCK_COMMENT)
            return &def->rules[i];
    return NULL;
}

void syntax_rebuild_state(SyntaxContext *ctx, const Document *doc, int changed_row) {
    if (!ctx || !ctx->def) return;

    const SyntaxRule *bc = find_block_comment_rule(ctx->def);
    if (!bc) { ctx->dirty = false; return; }

    int n = document_line_count(doc);

    /* 重新分配缓存数组 */
    if (ctx->cached_count < n + 1) {
        free(ctx->line_in_comment);
        ctx->line_in_comment = (bool*)calloc((size_t)(n + 2), sizeof(bool));
        ctx->cached_count    = n + 1;
    }

    /* 确定扫描起始行（changed_row=-1 全量从0开始） */
    int start = (changed_row >= 0 && changed_row < n) ? changed_row : 0;
    /* 若 start>0，沿用 start-1 行的结束状态（即 start 行的入口状态） */
    bool in_comment = (start > 0) ? ctx->line_in_comment[start] : false;

    const char *bs  = bc->block_start;
    const char *be  = bc->block_end;
    int bs_len      = (int)strlen(bs);
    int be_len      = (int)strlen(be);

    for (int row = start; row < n; row++) {
        /* 提前退出：在覆盖前先保存 row+1 的旧入口状态 */
        bool old_next = (row + 1 < ctx->cached_count)
                        ? ctx->line_in_comment[row + 1]
                        : false;

        ctx->line_in_comment[row] = in_comment;

        const char *line = document_get_line(doc, row);
        int         blen = document_get_line_len(doc, row);

        int i = 0;
        while (i < blen) {
            if (in_comment) {
                if (be_len > 0 && i + be_len <= blen &&
                    strncmp(line + i, be, (size_t)be_len) == 0) {
                    in_comment = false;
                    i += be_len;
                } else {
                    i++;
                }
            } else {
                if (bs_len > 0 && i + bs_len <= blen &&
                    strncmp(line + i, bs, (size_t)bs_len) == 0) {
                    in_comment = true;
                    i += bs_len;
                } else {
                    i++;
                }
            }
        }

        /* 若新计算的 row+1 入口状态与缓存一致，后续行不受影响，提前退出 */
        if (row > start && in_comment == old_next)
            break;
    }
    ctx->dirty = false;
}

/* ================================================================
 * 高亮一行：填充 out_attrs[0..line_len)
 * 规则优先级：数值大的优先。0 = 未分配（用 ATTR_NORMAL）。
 * ================================================================ */

/* 辅助：将 [start, end) 范围内优先级更高的属性写入 out_attrs */
static void fill_attrs(uint8_t *out_attrs, int attrs_len,
                        int start, int end,
                        uint8_t attr, int priority,
                        uint8_t *priority_buf) {
    for (int i = start; i < end && i < attrs_len; i++) {
        if (priority_buf[i] < (uint8_t)priority) {
            out_attrs[i]     = attr;
            priority_buf[i]  = (uint8_t)priority;
        }
    }
}

/* 判断 pos 是否在词边界（前/后均非单词字符） */
static int is_word_boundary(const char *line, int blen, int pos) {
    int before = (pos > 0)    && (isalnum((unsigned char)line[pos-1]) || line[pos-1] == '_');
    int after  = (pos < blen) && (isalnum((unsigned char)line[pos])   || line[pos]   == '_');
    return !before && !after;
}

/* strncmp 比较，不关心大小写 */
static int word_match(const char *kw, const char *line, int pos, int blen) {
    int klen = (int)strlen(kw);
    if (pos + klen > blen) return 0;
    if (strncmp(line + pos, kw, (size_t)klen) != 0) return 0;
    /* 词边界：前后不是单词字符 */
    int before_ok = (pos == 0) || !(isalnum((unsigned char)line[pos-1]) || line[pos-1] == '_');
    int after_ok  = (pos + klen == blen) ||
                    !(isalnum((unsigned char)line[pos+klen]) || line[pos+klen] == '_');
    return before_ok && after_ok;
}

void syntax_highlight_line(const SyntaxContext *ctx, const Document *doc,
                            int doc_row, uint8_t *out_attrs, int attrs_len) {
    if (!ctx || !ctx->def || !out_attrs || attrs_len <= 0) return;
    const SyntaxDef *def = ctx->def;

    const char *line = document_get_line(doc, doc_row);
    int         blen = document_get_line_len(doc, doc_row);
    if (blen <= 0) return;
    if (blen > attrs_len) blen = attrs_len;

    /* 优先级跟踪缓冲（0=未赋值） */
    uint8_t *prio = (uint8_t*)calloc((size_t)blen, sizeof(uint8_t));
    if (!prio) return;
    memset(out_attrs, 0, (size_t)attrs_len);

    /* ---- 1. 块注释（最高优先级，由状态机决定） ---- */
    const SyntaxRule *bc = find_block_comment_rule(def);
    if (bc) {
        bool in_comment = ctx->line_in_comment &&
                          doc_row < ctx->cached_count &&
                          ctx->line_in_comment[doc_row];
        const char *bs  = bc->block_start;
        const char *be  = bc->block_end;
        int bs_len      = (int)strlen(bs);
        int be_len      = (int)strlen(be);
        int i = 0;
        int seg_start = in_comment ? 0 : -1;

        while (i < blen) {
            if (in_comment) {
                if (be_len > 0 && i + be_len <= blen &&
                    strncmp(line + i, be, (size_t)be_len) == 0) {
                    int seg_end = i + be_len;
                    fill_attrs(out_attrs, attrs_len, seg_start, seg_end,
                               bc->attr, 100, prio);  /* 块注释优先级固定 100 */
                    in_comment = false;
                    seg_start  = -1;
                    i += be_len;
                } else { i++; }
            } else {
                if (bs_len > 0 && i + bs_len <= blen &&
                    strncmp(line + i, bs, (size_t)bs_len) == 0) {
                    in_comment = true;
                    seg_start  = i;
                    i += bs_len;
                } else { i++; }
            }
        }
        if (in_comment && seg_start >= 0) {
            fill_attrs(out_attrs, attrs_len, seg_start, blen, bc->attr, 100, prio);
        }
    }

    /* ---- 2. 其他规则（按 priority 大小排序，不依赖遍历顺序） ---- */
    for (int ri = 0; ri < def->rule_count; ri++) {
        const SyntaxRule *r = &def->rules[ri];
        if (r->type == RULE_BLOCK_COMMENT) continue;  /* 已处理 */

        int i = 0;
        while (i < blen) {
            /* 跳过已被块注释占据的位置 */
            if (prio[i] >= 100) { i++; continue; }

            switch (r->type) {

            case RULE_LINE_COMMENT: {
                int plen = (int)strlen(r->prefix);
                if (plen > 0 && i + plen <= blen &&
                    strncmp(line + i, r->prefix, (size_t)plen) == 0) {
                    fill_attrs(out_attrs, attrs_len, i, blen, r->attr, r->priority, prio);
                    i = blen;
                } else { i++; }
                break;
            }

            case RULE_LINE_START:
                if (i == 0) {
                    int plen = (int)strlen(r->prefix);
                    if (plen > 0 && blen >= plen &&
                        strncmp(line, r->prefix, (size_t)plen) == 0) {
                        fill_attrs(out_attrs, attrs_len, 0, blen, r->attr, r->priority, prio);
                    }
                }
                i = blen;  /* 只检查行首一次 */
                break;

            case RULE_STRING: {
                if (line[i] != r->delimiter) { i++; break; }
                int j = i + 1;
                while (j < blen) {
                    if (r->escape && line[j] == r->escape && j + 1 < blen) {
                        j += 2;  /* 跳过转义字符 */
                    } else if (line[j] == r->delimiter) {
                        j++;
                        break;
                    } else { j++; }
                }
                fill_attrs(out_attrs, attrs_len, i, j, r->attr, r->priority, prio);
                i = j;
                break;
            }

            case RULE_KEYWORD: {
                /* 仅在单词开始时尝试 */
                if (i > 0 && (isalnum((unsigned char)line[i-1]) || line[i-1] == '_')) {
                    i++; break;
                }
                int matched = 0;
                for (int ki = 0; ki < r->keyword_count; ki++) {
                    int klen = (int)strlen(r->keywords[ki]);
                    if (word_match(r->keywords[ki], line, i, blen)) {
                        fill_attrs(out_attrs, attrs_len, i, i + klen,
                                   r->attr, r->priority, prio);
                        i += klen;
                        matched = 1;
                        break;
                    }
                }
                if (!matched) i++;
                break;
            }

            case RULE_NUMBER: {
                /* 数字：不在标识符中间，以数字开始 */
                if (i > 0 && (isalnum((unsigned char)line[i-1]) || line[i-1] == '_')) {
                    i++; break;
                }
                if (!isdigit((unsigned char)line[i])) { i++; break; }
                int j = i;
                /* 十六进制 0x... */
                if (line[j] == '0' && j + 1 < blen &&
                    (line[j+1] == 'x' || line[j+1] == 'X')) {
                    j += 2;
                    while (j < blen && isxdigit((unsigned char)line[j])) j++;
                } else {
                    while (j < blen && isdigit((unsigned char)line[j])) j++;
                    /* 小数部分 */
                    if (j < blen && line[j] == '.' && j + 1 < blen &&
                        isdigit((unsigned char)line[j+1])) {
                        j++;
                        while (j < blen && isdigit((unsigned char)line[j])) j++;
                    }
                    /* 指数部分 */
                    if (j < blen && (line[j] == 'e' || line[j] == 'E')) {
                        j++;
                        if (j < blen && (line[j] == '+' || line[j] == '-')) j++;
                        while (j < blen && isdigit((unsigned char)line[j])) j++;
                    }
                }
                /* 后缀 lLuUfF */
                while (j < blen && strchr("lLuUfF", line[j])) j++;
                fill_attrs(out_attrs, attrs_len, i, j, r->attr, r->priority, prio);
                i = j;
                break;
            }

            case RULE_REGEX: {
                if (!r->pattern[0]) { i++; break; }
                int end_pos;
                int found = regex_search(r->pattern, line, blen, i, &end_pos);
                if (found >= 0 && end_pos > found) {
                    fill_attrs(out_attrs, attrs_len, found, end_pos,
                               r->attr, r->priority, prio);
                    i = end_pos;
                } else {
                    i = blen;  /* 无匹配，跳出 */
                }
                break;
            }

            default:
                i++;
                break;
            }
        }
    }

    free(prio);
}

/* ================================================================
 * SyntaxContext 生命周期
 * ================================================================ */

void syntax_ctx_init(SyntaxContext *ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(SyntaxContext));
}

void syntax_ctx_free(SyntaxContext *ctx) {
    if (!ctx) return;
    free(ctx->line_in_comment);
    ctx->line_in_comment = NULL;
    ctx->cached_count    = 0;
    /* def 由调用方管理（syntax_free） */
}

/* ================================================================
 * 语言列表扫描（Phase 7）
 * ================================================================ */

int syntax_list_languages(const char *syntax_dir,
                           SyntaxLangInfo *out, int max_count) {
    if (!syntax_dir || !syntax_dir[0] || max_count <= 0) return 0;

#ifdef _WIN32
    char pattern[600];
    snprintf(pattern, sizeof(pattern), "%s\\*.ini", syntax_dir);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;

    int count = 0;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (count >= max_count) break;

        snprintf(out[count].path, sizeof(out[count].path),
                 "%s\\%s", syntax_dir, fd.cFileName);

        out[count].name[0] = '\0';
        FILE *fp = fopen(out[count].path, "r");
        if (fp) {
            char line[256];
            while (fgets(line, sizeof(line), fp)) {
                int len = (int)strlen(line);
                while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n'))
                    line[--len] = '\0';
                if (strncmp(line, "name", 4) == 0) {
                    char *eq = strchr(line, '=');
                    if (eq) {
                        eq++;
                        while (*eq == ' ' || *eq == '\t') eq++;
                        strncpy(out[count].name, eq,
                                sizeof(out[count].name) - 1);
                        out[count].name[sizeof(out[count].name) - 1] = '\0';
                        break;
                    }
                }
            }
            fclose(fp);
        }

        if (!out[count].name[0]) {
            strncpy(out[count].name, fd.cFileName,
                    sizeof(out[count].name) - 1);
            out[count].name[sizeof(out[count].name) - 1] = '\0';
            char *dot = strrchr(out[count].name, '.');
            if (dot) *dot = '\0';
        }

        count++;
    } while (FindNextFileA(h, &fd));

    FindClose(h);
    return count;

#else
    (void)out;
    return 0;
#endif
}
