/*
 * config.c — 配置管理实现
 * 简单 INI 格式解析（key=value，# 开头为注释）
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "util.h"

/* ================================================================
 * 内部配置数据
 * ================================================================ */
/* 语法扩展名覆盖条目 */
typedef struct {
    char ext[32];      /* 扩展名，不含点，小写 */
    char ini_name[64]; /* INI 文件名，不含路径 */
} SyntaxExtEntry;

static struct {
    WrapMode       wrap_mode;
    bool           show_lineno;
    int            tab_size;
    long           max_file_size;
    char           recent[CONFIG_RECENT_MAX][512];
    int            recent_count;
    SyntaxExtEntry syntax_ext[CONFIG_SYNTAX_EXT_MAX];
    int            syntax_ext_count;
} g_cfg = {
    /* 硬编码默认值 */
    .wrap_mode        = WRAP_CHAR,
    .show_lineno      = false,
    .tab_size         = 4,
    .max_file_size    = 512 * 1024L,
    .recent_count     = 0,
    .syntax_ext_count = 0
};

/* ================================================================
 * INI 解析
 * ================================================================ */

static void parse_line(const char *line) {
    /* 跳过空行和注释 */
    if (!line || line[0] == '#' || line[0] == ';' || line[0] == '\0') return;

    /* 找 = 分隔符 */
    const char *eq = strchr(line, '=');
    if (!eq) return;

    char key[64] = {0};
    char val[512] = {0};
    int klen = (int)(eq - line);
    if (klen <= 0 || klen >= (int)sizeof(key)) return;
    strncpy(key, line, (size_t)klen);

    strncpy(val, eq + 1, sizeof(val) - 1);
    /* 去掉 key 和 val 末尾空白 */
    str_trim_crlf(val);

    /* 解析各配置项 */
    if (strcmp(key, "wrap_mode") == 0) {
        g_cfg.wrap_mode = (atoi(val) == 1) ? WRAP_NONE : WRAP_CHAR;
    } else if (strcmp(key, "show_lineno") == 0) {
        g_cfg.show_lineno = (atoi(val) != 0);
    } else if (strcmp(key, "tab_size") == 0) {
        int ts = atoi(val);
        if (ts >= 1 && ts <= 16) g_cfg.tab_size = ts;
    } else if (strcmp(key, "max_file_size") == 0) {
        long sz = atol(val);
        if (sz > 0) g_cfg.max_file_size = sz;
    } else if (str_starts_with(key, "recent")) {
        /* recent0, recent1, ... */
        int idx = atoi(key + 6);
        if (idx >= 0 && idx < CONFIG_RECENT_MAX && val[0]) {
            strncpy(g_cfg.recent[idx], val, 511);
            if (idx >= g_cfg.recent_count)
                g_cfg.recent_count = idx + 1;
        }
    } else if (str_starts_with(key, "syntax_ext_")) {
        /* syntax_ext_<ext>=<ini_name>，如 syntax_ext_xyz=c.ini */
        const char *ext_part = key + 11;  /* 跳过 "syntax_ext_" */
        if (ext_part[0] && g_cfg.syntax_ext_count < CONFIG_SYNTAX_EXT_MAX) {
            int i = g_cfg.syntax_ext_count++;
            strncpy(g_cfg.syntax_ext[i].ext, ext_part,
                    sizeof(g_cfg.syntax_ext[i].ext) - 1);
            /* 强制小写 */
            for (int j = 0; g_cfg.syntax_ext[i].ext[j]; j++)
                g_cfg.syntax_ext[i].ext[j] =
                    (char)tolower((unsigned char)g_cfg.syntax_ext[i].ext[j]);
            strncpy(g_cfg.syntax_ext[i].ini_name, val,
                    sizeof(g_cfg.syntax_ext[i].ini_name) - 1);
        }
    }
}

/* ================================================================
 * 加载/保存
 * ================================================================ */

void config_load(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return;

    char buf[600];
    while (fgets(buf, sizeof(buf), fp)) {
        str_trim_crlf(buf);
        parse_line(buf);
    }
    fclose(fp);
}

/* 确定用户配置路径 */
static void user_config_path(char *out, int out_size) {
    char dir[512];
    get_user_config_dir(dir, sizeof(dir));
    path_join(dir, "edit.ini", out, out_size);
}

void config_load_auto(void) {
    /* 优先级 1：用户配置 */
    char user_path[600];
    user_config_path(user_path, sizeof(user_path));
    if (file_exists(user_path)) {
        config_load(user_path);
        return;
    }

    /* 优先级 2：程序目录 config.ini */
    char exe_dir[512], local_path[600];
    get_exe_dir(exe_dir, sizeof(exe_dir));
    path_join(exe_dir, "config.ini", local_path, sizeof(local_path));
    if (file_exists(local_path)) {
        config_load(local_path);
        return;
    }

    /* 优先级 3：当前目录 config.ini */
    if (file_exists("config.ini")) {
        config_load("config.ini");
    }
    /* 否则使用内置默认值 */
}

void config_save(const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) return;

    fprintf(fp, "# DOS Text Editor 配置文件\n");
    fprintf(fp, "wrap_mode=%d\n", g_cfg.wrap_mode == WRAP_NONE ? 1 : 0);
    fprintf(fp, "show_lineno=%d\n", g_cfg.show_lineno ? 1 : 0);
    fprintf(fp, "tab_size=%d\n", g_cfg.tab_size);
    fprintf(fp, "max_file_size=%ld\n", g_cfg.max_file_size);

    for (int i = 0; i < g_cfg.recent_count; i++) {
        if (g_cfg.recent[i][0])
            fprintf(fp, "recent%d=%s\n", i, g_cfg.recent[i]);
    }
    fclose(fp);
}

void config_save_auto(void) {
    char user_path[600];
    user_config_path(user_path, sizeof(user_path));
    config_save(user_path);
}

/* ================================================================
 * Getter / Setter
 * ================================================================ */

WrapMode config_get_wrap_mode(void)          { return g_cfg.wrap_mode; }
void     config_set_wrap_mode(WrapMode m)    { g_cfg.wrap_mode = m; }

bool     config_get_show_lineno(void)        { return g_cfg.show_lineno; }
void     config_set_show_lineno(bool v)      { g_cfg.show_lineno = v; }

int      config_get_tab_size(void)           { return g_cfg.tab_size; }
void     config_set_tab_size(int size)       { if (size >= 1) g_cfg.tab_size = size; }

long     config_get_max_file_size(void)      { return g_cfg.max_file_size; }
void     config_set_max_file_size(long size) { if (size > 0) g_cfg.max_file_size = size; }

void config_add_recent(const char *path) {
    if (!path || !path[0]) return;

    /* 若已存在，先移除旧条目 */
    int found = -1;
    for (int i = 0; i < g_cfg.recent_count; i++) {
        if (strcmp(g_cfg.recent[i], path) == 0) { found = i; break; }
    }
    if (found >= 0) {
        /* 向前移 */
        for (int i = found; i < g_cfg.recent_count - 1; i++)
            strcpy(g_cfg.recent[i], g_cfg.recent[i + 1]);
        g_cfg.recent_count--;
    }

    /* 移位，插到最前 */
    int new_count = MIN(g_cfg.recent_count + 1, CONFIG_RECENT_MAX);
    for (int i = new_count - 1; i > 0; i--)
        strcpy(g_cfg.recent[i], g_cfg.recent[i - 1]);
    strncpy(g_cfg.recent[0], path, 511);
    g_cfg.recent_count = new_count;
}

const char* config_get_recent(int idx) {
    if (idx < 0 || idx >= g_cfg.recent_count) return NULL;
    return g_cfg.recent[idx][0] ? g_cfg.recent[idx] : NULL;
}

int config_get_recent_count(void) {
    return g_cfg.recent_count;
}

const char* config_get_syntax_override(const char *ext) {
    if (!ext || !ext[0]) return NULL;
    for (int i = 0; i < g_cfg.syntax_ext_count; i++) {
        if (strcmp(g_cfg.syntax_ext[i].ext, ext) == 0)
            return g_cfg.syntax_ext[i].ini_name;
    }
    return NULL;
}
