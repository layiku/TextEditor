/*
 * document.h — 文档数据模型接口
 * 负责文本存储（行数组）、基本编辑操作、撤销/重做栈
 * 纯逻辑层，不依赖显示或平台
 */
#pragma once

#include <stdbool.h>

/* ================================================================
 * 行结构：存储单行文本（不含 \n），动态扩容
 * ================================================================ */
typedef struct {
    char *text;  /* 行内容缓冲区，以 \0 结尾（但 len 是有效长度，不含 \0） */
    int   len;   /* 有效字符数 */
    int   cap;   /* 已分配字节数（含 \0 位） */
} Line;

/* ================================================================
 * 撤销记录类型
 * 所有编辑操作归一化为"在位置 (row,col) 插入文本" 或 "删除文本"
 * 换行 (Enter) = 插入 "\n"；行首 Backspace = 删除 "\n"
 * ================================================================ */
typedef enum {
    UNDO_INS = 0,  /* 在 (row,col) 插入了 text[0..len-1] */
    UNDO_DEL = 1   /* 从 (row,col) 删除了 text[0..len-1] */
} UndoType;

/* 撤销/重做所需的最小信息 */
typedef struct {
    UndoType type;
    int   row, col;   /* 操作位置（文档逻辑坐标） */
    char *text;       /* 被插入或被删除的文本（可含 \n） */
    int   len;        /* text 长度 */
    /* 合并标志：与前一条记录合并（连续输入同行相邻字符时置 1） */
    bool  merged;
} UndoRecord;

/* ================================================================
 * 文档结构
 * ================================================================ */
#define DOCUMENT_MAX_LINES  65536   /* 最大行数 */
#define UNDO_STACK_SIZE     1024    /* 最大撤销步数（每步可含多字符） */
#define LINE_INIT_CAP       64      /* 新行初始容量 */

/* 文件编码（在此声明以避免循环包含 encoding.h） */
typedef enum {
    DOC_ENC_UTF8 = 0,  /* UTF-8（默认） */
    DOC_ENC_GBK  = 1,  /* GBK / GB2312 / CP936 */
} DocEncoding;

typedef struct {
    /* 行存储 */
    Line *lines;       /* 行数组（动态分配） */
    int   line_count;  /* 有效行数（始终 >= 1） */
    int   line_cap;    /* 行数组已分配容量 */

    /* 撤销栈：栈顶 = undo_top-1，初始为 0 */
    UndoRecord undo_stack[UNDO_STACK_SIZE];
    int  undo_top;       /* 下一条入栈位置（[0, undo_top) 是有效记录） */
    int  undo_redo_top;  /* redo 上界：[undo_top, undo_redo_top) 是可 redo 区 */
    int  undo_save_idx;  /* 保存点：执行 save 时记录，回到此点时 modified=false */

    /* 状态 */
    bool modified;       /* 是否有未保存修改 */
    char filepath[512];  /* 当前文件路径，空串 = 未命名新文件 */

    /* 编码信息（Phase 6：加载时检测，保存时按原编码写回） */
    DocEncoding encoding;   /* 检测到的文件编码 */
    bool        has_utf8_bom; /* 原始文件是否含 UTF-8 BOM */
} Document;

/* ================================================================
 * 生命周期
 * ================================================================ */
Document* document_new(void);
void      document_free(Document *doc);
void      document_clear(Document *doc);  /* 清空内容，保留结构（用于新建） */

/* ================================================================
 * 文件 I/O
 * ================================================================ */
int document_load(Document *doc, const char *path);  /* 0=成功，-1=失败 */
int document_save(Document *doc, const char *path);  /* 0=成功，-1=失败 */

/* ================================================================
 * 基本编辑（均自动记录撤销） 
 * ================================================================ */
/* 在 (row, col) 处插入单个可打印字符（非 \n）
 * merge=1 时尝试与前一条撤销记录合并（连续输入优化） */
void document_insert_char(Document *doc, int row, int col, char c, bool merge);

/* 在 (row, col) 处插入换行（等价于在流中插入 '\n'）
 * 将当前行 col 之后的内容拆到新行 row+1 */
void document_break_line(Document *doc, int row, int col);

/* 删除 (row, col) 处的字符（Delete 键）：若 col==len[row]，合并下一行 */
void document_delete_char(Document *doc, int row, int col);

/* 删除 (row, col-1) 处的字符（Backspace 键）：若 col==0，合并上一行 */
void document_backspace(Document *doc, int row, int col);

/* 批量操作（粘贴、选中删除等），合并为一条撤销记录 */
/* 在 (row, col) 插入多字符文本（可含 \n），返回插入后光标位置 */
void document_insert_text(Document *doc, int row, int col,
                          const char *text, int len,
                          int *out_row, int *out_col);

/* 删除 (r1,c1) 到 (r2,c2) 的范围（r2>r1 或 r2==r1&&c2>c1）
 * 将被删内容写入 out_buf（调用方分配，out_buf_size 需足够大）
 * 返回实际写入字节数 */
int document_delete_range(Document *doc,
                          int r1, int c1, int r2, int c2,
                          char *out_buf, int out_buf_size);

/* ================================================================
 * 撤销 / 重做
 * 执行成功时将新光标位置写入 *out_row, *out_col，返回 0；
 * 无法撤销/重做时返回 -1
 * ================================================================ */
int document_undo(Document *doc, int *out_row, int *out_col);
int document_redo(Document *doc, int *out_row, int *out_col);

/* 记录当前状态为保存点（保存文件后调用） */
void document_mark_saved(Document *doc);

/* ================================================================
 * 查询接口（只读）
 * ================================================================ */
/* 获取第 row 行的文本指针（只读，不可修改） */
const char* document_get_line(const Document *doc, int row);

/* 获取第 row 行的字符数 */
int document_get_line_len(const Document *doc, int row);

/* 获取文档总行数 */
int document_line_count(const Document *doc);

/* 将光标位置规范化到文档有效范围 */
void document_clamp_pos(const Document *doc, int *row, int *col);
