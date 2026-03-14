/*
 * config.h — 配置管理接口
 * 配置文件格式：简单 INI（key=value，# 注释）
 * 加载优先级：① 用户配置（~/.editrc 或 %APPDATA%\edit.ini）
 *             ② 程序目录的 config.ini
 *             ③ 内置硬编码默认值
 */
#pragma once

#include <stdbool.h>
#include "types.h"

#define CONFIG_RECENT_MAX 8   /* 最近文件列表最大条数 */

/* ================================================================
 * 配置加载/保存
 * ================================================================ */
/* 按优先级自动查找并加载配置文件（程序启动时调用） */
void config_load_auto(void);

/* 从指定路径加载（失败则保持现有值） */
void config_load(const char *path);

/* 保存到用户配置文件路径 */
void config_save_auto(void);

/* 保存到指定路径 */
void config_save(const char *path);

/* ================================================================
 * 配置项 getter/setter
 * ================================================================ */
WrapMode    config_get_wrap_mode(void);
void        config_set_wrap_mode(WrapMode m);

bool        config_get_show_lineno(void);
void        config_set_show_lineno(bool v);

int         config_get_tab_size(void);
void        config_set_tab_size(int size);

long        config_get_max_file_size(void);
void        config_set_max_file_size(long size);

/* ================================================================
 * 最近文件列表
 * ================================================================ */
void        config_add_recent(const char *path);
const char* config_get_recent(int idx);   /* 返回第 idx 条（0=最新），NULL=不存在 */
int         config_get_recent_count(void);

/* ================================================================
 * 语法扩展名覆盖（syntax_ext_<ext>=<ini_name>）
 * 例如 config.ini 中写 syntax_ext_xyz=c.ini 后，打开 .xyz 文件使用 C 语法
 * ================================================================ */
#define CONFIG_SYNTAX_EXT_MAX 32

/* 返回扩展名 ext（不含点，小写）对应的 ini 文件名（不含路径），
 * 若未配置则返回 NULL。 */
const char* config_get_syntax_override(const char *ext);

/* ================================================================
 * 语法扩展名覆盖映射
 * 配置格式：syntax_ext_<ext>=<ini_filename>，例如：
 *   syntax_ext_xyz=c.ini
 * 通过 config_get_syntax_override("xyz") 取得 "c.ini"（不含路径）
 * 返回 NULL 表示无配置
 * ================================================================ */
#define CONFIG_SYNTAX_EXT_MAX 32  /* 最多可配置的扩展名条数 */

const char* config_get_syntax_override(const char *ext);
