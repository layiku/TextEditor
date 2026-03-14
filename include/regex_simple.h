/*
 * regex_simple.h — 轻量正则表达式引擎
 *
 * 支持子集（够语法高亮使用）：
 *   .        任意单字符
 *   *        前一元素重复 0 次或多次（贪婪）
 *   +        前一元素重复 1 次或多次（贪婪）
 *   ?        前一元素重复 0 次或 1 次
 *   [abc]    字符类（支持 a-z 范围，^ 取反）
 *   \d       数字 [0-9]
 *   \w       单词字符 [a-zA-Z0-9_]
 *   \s       空白字符 [ \t\r\n]
 *   \b       词边界（零宽）
 *   ^        行首锚点（仅在模式开头有效）
 *   $        行尾锚点（仅在模式末尾有效）
 *
 * 不支持：捕获分组、回溯限制、Unicode 感知（按字节匹配）
 */
#pragma once

/*
 * 从 text[pos] 开始尝试匹配 pattern。
 *
 * 参数：
 *   pattern  — 正则表达式字符串
 *   text     — 待匹配文本（UTF-8 字节流，不需要以 \0 结尾）
 *   len      — text 的有效字节数
 *   pos      — 起始位置（字节偏移）
 *   out_end  — 匹配成功时输出匹配结束位置（exclusive），可为 NULL
 *
 * 返回：1 = 匹配成功，0 = 不匹配
 */
int regex_match(const char *pattern, const char *text, int len,
                int pos, int *out_end);

/*
 * 在 text[pos..len) 中搜索第一个匹配。
 *
 * 返回：匹配起始位置（>=pos），-1 = 无匹配
 * out_end 输出匹配结束位置（exclusive）
 */
int regex_search(const char *pattern, const char *text, int len,
                 int pos, int *out_end);
