/*
 * document.c — 文档数据模型实现
 * 行数组 + 动态扩容 + 撤销/重做栈
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "document.h"
#include "encoding.h"
#include "utf8.h"
#include "util.h"

/* ================================================================
 * 内部辅助：行操作（不记录撤销，供 undo/redo 的原始操作使用）
 * ================================================================ */

static void line_ensure_cap(Line *line, int need) {
    if (need <= line->cap) return;
    int new_cap = line->cap * 2;
    if (new_cap < need) new_cap = need + 16;
    line->text = (char*)safe_realloc(line->text, (size_t)new_cap);
    line->cap  = new_cap;
}

static void line_init(Line *line) {
    line->cap  = LINE_INIT_CAP;
    line->text = (char*)safe_malloc((size_t)LINE_INIT_CAP);
    line->text[0] = '\0';
    line->len  = 0;
}

static void line_free(Line *line) {
    free(line->text);
    line->text = NULL;
    line->len  = 0;
    line->cap  = 0;
}

/* col must be in [0, line->len] */
static void line_insert_char(Line *line, int col, char c) {
    line_ensure_cap(line, line->len + 2);
    memmove(line->text + col + 1, line->text + col, (size_t)(line->len - col + 1));
    line->text[col] = c;
    line->len++;
}

static char line_delete_char(Line *line, int col) {
    char c = line->text[col];
    memmove(line->text + col, line->text + col + 1, (size_t)(line->len - col));
    line->len--;
    return c;
}

static void line_append(Line *line, const char *s, int len) {
    line_ensure_cap(line, line->len + len + 1);
    memcpy(line->text + line->len, s, (size_t)len);
    line->len += len;
    line->text[line->len] = '\0';
}

/* ----------------------------------------------------------------
 * 文档行数组管理
 * ---------------------------------------------------------------- */

static void doc_ensure_line_cap(Document *doc, int need) {
    if (need <= doc->line_cap) return;
    int new_cap = doc->line_cap * 2;
    if (new_cap < need) new_cap = need + 8;
    doc->lines = (Line*)safe_realloc(doc->lines, (size_t)new_cap * sizeof(Line));
    doc->line_cap = new_cap;
}

static void doc_insert_empty_line(Document *doc, int row) {
    doc_ensure_line_cap(doc, doc->line_count + 1);
    memmove(doc->lines + row + 1, doc->lines + row,
            (size_t)(doc->line_count - row) * sizeof(Line));
    line_init(&doc->lines[row]);
    doc->line_count++;
}

static void doc_remove_line(Document *doc, int row) {
    line_free(&doc->lines[row]);
    memmove(doc->lines + row, doc->lines + row + 1,
            (size_t)(doc->line_count - row - 1) * sizeof(Line));
    doc->line_count--;
}

/* ================================================================
 * 原始编辑操作（不记录撤销，供内部使用）
 * ================================================================ */

static void raw_insert_char(Document *doc, int row, int col, char c) {
    line_insert_char(&doc->lines[row], col, c);
}

/* 在 (row, col) 插入换行：将 line[row][col..end] 拆到新 line[row+1] */
static void raw_break_line(Document *doc, int row, int col) {
    Line *cur = &doc->lines[row];
    int   tail_len = cur->len - col;
    char *tail = (char*)safe_malloc((size_t)(tail_len + 1));
    memcpy(tail, cur->text + col, (size_t)tail_len);
    tail[tail_len] = '\0';

    /* 截断当前行 */
    cur->text[col] = '\0';
    cur->len = col;

    /* 插入新行 */
    doc_insert_empty_line(doc, row + 1);
    line_append(&doc->lines[row + 1], tail, tail_len);
    free(tail);
}

/* 合并 row 和 row+1（将 row+1 的内容追加到 row，删除 row+1） */
static void raw_merge_line(Document *doc, int row) {
    Line *cur  = &doc->lines[row];
    Line *next = &doc->lines[row + 1];
    line_append(cur, next->text, next->len);
    doc_remove_line(doc, row + 1);
}

/* 删除 (row, col) 处的字符（col 在 [0, len-1]） */
static char raw_delete_char(Document *doc, int row, int col) {
    return line_delete_char(&doc->lines[row], col);
}

/* ================================================================
 * 撤销栈操作
 * ================================================================ */

/* 推入一条撤销记录（清除 redo 区） */
static void undo_push(Document *doc, UndoType type,
                      int row, int col, const char *text, int len, bool merged) {
    /* 清除 redo 历史 */
    for (int i = doc->undo_top; i < doc->undo_redo_top; i++)
        free(doc->undo_stack[i].text);
    doc->undo_redo_top = doc->undo_top;

    /* 栈满时丢弃最老的记录 */
    if (doc->undo_top >= UNDO_STACK_SIZE) {
        free(doc->undo_stack[0].text);
        memmove(doc->undo_stack, doc->undo_stack + 1,
                (size_t)(UNDO_STACK_SIZE - 1) * sizeof(UndoRecord));
        doc->undo_top--;
        if (doc->undo_save_idx > 0) doc->undo_save_idx--;
    }

    UndoRecord *r = &doc->undo_stack[doc->undo_top];
    r->type   = type;
    r->row    = row;
    r->col    = col;
    r->len    = len;
    r->text   = (char*)safe_malloc((size_t)(len + 1));
    memcpy(r->text, text, (size_t)len);
    r->text[len] = '\0';
    r->merged = merged;

    doc->undo_top++;
    doc->undo_redo_top = doc->undo_top;
    doc->modified = true;
}

/* 尝试与栈顶合并（相同 row，当前 col == 上次 col+1，类型相同为 INS） */
static bool try_merge(Document *doc, int row, int col) {
    if (doc->undo_top == 0) return false;
    UndoRecord *top = &doc->undo_stack[doc->undo_top - 1];
    if (top->type != UNDO_INS) return false;
    if (top->row  != row)      return false;
    if (top->col + top->len != col) return false;
    return true;
}

/* 追加字符到栈顶 INS 记录（合并连续输入） */
static void undo_append_char(Document *doc, char c) {
    UndoRecord *top = &doc->undo_stack[doc->undo_top - 1];
    top->text = (char*)safe_realloc(top->text, (size_t)(top->len + 2));
    top->text[top->len] = c;
    top->len++;
    top->text[top->len] = '\0';
}

/* ================================================================
 * 重放：在文档中应用文本插入（含 \n）
 * 返回操作后的光标位置
 * ================================================================ */
static void apply_insert(Document *doc, int row, int col,
                         const char *text, int len,
                         int *out_row, int *out_col) {
    int cur_row = row, cur_col = col;
    for (int i = 0; i < len; i++) {
        if (text[i] == '\n') {
            raw_break_line(doc, cur_row, cur_col);
            cur_row++;
            cur_col = 0;
        } else {
            raw_insert_char(doc, cur_row, cur_col, text[i]);
            cur_col++;
        }
    }
    if (out_row) *out_row = cur_row;
    if (out_col) *out_col = cur_col;
}

/* 重放：删除从 (row,col) 开始共 len 个逻辑字符（含 \n）
 * 返回被删除的文本（调用方负责 free） */
static char* apply_delete(Document *doc, int row, int col, int len) {
    char *buf = (char*)safe_malloc((size_t)(len + 1));
    int   bi  = 0;
    int   cur_row = row, cur_col = col;

    for (int i = 0; i < len; i++) {
        if (cur_row >= doc->line_count) break;
        Line *line = &doc->lines[cur_row];
        if (cur_col >= line->len) {
            /* 当前在行尾，删除换行（合并下一行） */
            buf[bi++] = '\n';
            raw_merge_line(doc, cur_row);
            /* cur_row, cur_col 不变，已合并 */
        } else {
            buf[bi++] = raw_delete_char(doc, cur_row, cur_col);
            /* cur_col 不变，下一个字符现在到了同位置 */
        }
    }
    buf[bi] = '\0';
    return buf;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

Document* document_new(void) {
    Document *doc = (Document*)safe_malloc(sizeof(Document));
    memset(doc, 0, sizeof(Document));

    doc->line_cap   = 16;
    doc->lines      = (Line*)safe_malloc((size_t)doc->line_cap * sizeof(Line));
    /* 新文档有一个空行 */
    line_init(&doc->lines[0]);
    doc->line_count  = 1;
    doc->undo_save_idx = 0;
    doc->modified    = false;
    doc->filepath[0] = '\0';
    return doc;
}

void document_free(Document *doc) {
    if (!doc) return;
    for (int i = 0; i < doc->line_count; i++)
        line_free(&doc->lines[i]);
    free(doc->lines);

    /* 释放撤销栈 */
    for (int i = 0; i < doc->undo_redo_top; i++)
        free(doc->undo_stack[i].text);

    free(doc);
}

void document_clear(Document *doc) {
    for (int i = 0; i < doc->line_count; i++)
        line_free(&doc->lines[i]);
    line_init(&doc->lines[0]);
    doc->line_count = 1;

    for (int i = 0; i < doc->undo_redo_top; i++)
        free(doc->undo_stack[i].text);
    doc->undo_top       = 0;
    doc->undo_redo_top  = 0;
    doc->undo_save_idx  = 0;
    doc->modified       = false;
}

/* ----------------------------------------------------------------
 * 文件 I/O
 * ---------------------------------------------------------------- */

int document_load(Document *doc, const char *path) {
    /* 二进制方式读取全文，用于编码检测和 GBK 转换 */
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (fsize < 0) { fclose(fp); return -1; }

    char *raw = (char*)malloc((size_t)fsize + 4);
    if (!raw) { fclose(fp); return -1; }
    long nread = (long)fread(raw, 1, (size_t)fsize, fp);
    fclose(fp);
    raw[nread] = '\0';

    /* 检测编码 */
    FileEncoding fenc = encoding_detect(raw, (int)nread);
    doc->encoding     = (fenc == ENC_GBK) ? DOC_ENC_GBK : DOC_ENC_UTF8;
    doc->has_utf8_bom = (nread >= 3 &&
                         (unsigned char)raw[0] == 0xEF &&
                         (unsigned char)raw[1] == 0xBB &&
                         (unsigned char)raw[2] == 0xBF);

    const char *src     = raw;
    int         src_len = (int)nread;
    char       *converted = NULL;

    if (fenc == ENC_GBK) {
        /* GBK → UTF-8 */
        int cap = src_len * 3 + 4;
        converted = (char*)malloc((size_t)cap);
        if (converted) {
            int r = gbk_to_utf8_buf(raw, src_len, converted, cap);
            if (r >= 0) { src = converted; src_len = r; }
        }
    } else if (doc->has_utf8_bom) {
        /* 跳过 BOM */
        src     += 3;
        src_len -= 3;
    }

    /* 按行分割（以 \n 为分隔符，兼容 \r\n） */
    document_clear(doc);
    doc->lines[0].len     = 0;
    doc->lines[0].text[0] = '\0';
    int cur_row = 0;
    int i = 0;

    while (i < src_len) {
        /* 找下一个换行 */
        int j = i;
        while (j < src_len && src[j] != '\n') j++;

        int line_len = j - i;
        /* 去除行尾 \r */
        if (line_len > 0 && src[i + line_len - 1] == '\r') line_len--;

        if (cur_row >= DOCUMENT_MAX_LINES - 1) break;
        line_append(&doc->lines[cur_row], src + i, line_len);

        if (j < src_len) {
            /* 有换行：开新行 */
            doc_insert_empty_line(doc, cur_row + 1);
            cur_row++;
        }
        i = j + 1;
    }

    free(raw);
    if (converted) free(converted);

    strncpy(doc->filepath, path, sizeof(doc->filepath) - 1);
    doc->modified      = false;
    doc->undo_save_idx = 0;
    return 0;
}

int document_save(Document *doc, const char *path) {
    char tmp_path[516];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    /* 先将文档内容合并为 UTF-8 字节流 */
    int total_utf8 = 0;
    for (int i = 0; i < doc->line_count; i++)
        total_utf8 += doc->lines[i].len + 1;  /* +1 for \n */
    total_utf8 += 4;

    char *utf8_buf = (char*)malloc((size_t)total_utf8);
    if (!utf8_buf) return -1;

    int pos = 0;
    for (int i = 0; i < doc->line_count; i++) {
        int len = doc->lines[i].len;
        memcpy(utf8_buf + pos, doc->lines[i].text, (size_t)len);
        pos += len;
        if (i < doc->line_count - 1)
            utf8_buf[pos++] = '\n';
    }
    utf8_buf[pos] = '\0';

    /* 若原始编码为 GBK，转换回 GBK 写出 */
    const char *write_buf = utf8_buf;
    int         write_len = pos;
    char       *gbk_buf   = NULL;

    if (doc->encoding == DOC_ENC_GBK) {
        int cap = pos * 2 + 4;
        gbk_buf = (char*)malloc((size_t)cap);
        if (gbk_buf) {
            int r = utf8_to_gbk_buf(utf8_buf, pos, gbk_buf, cap);
            if (r >= 0) { write_buf = gbk_buf; write_len = r; }
        }
    }

    FILE *fp = fopen(tmp_path, "wb");
    if (!fp) { free(utf8_buf); free(gbk_buf); return -1; }

    /* UTF-8 BOM */
    if (doc->encoding == DOC_ENC_UTF8 && doc->has_utf8_bom) {
        unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
        fwrite(bom, 1, 3, fp);
    }

    fwrite(write_buf, 1, (size_t)write_len, fp);
    fclose(fp);
    free(utf8_buf);
    free(gbk_buf);

    remove(path);
    if (rename(tmp_path, path) != 0) {
        remove(tmp_path);
        return -1;
    }

    strncpy(doc->filepath, path, sizeof(doc->filepath) - 1);
    document_mark_saved(doc);
    return 0;
}

/* ----------------------------------------------------------------
 * 编辑接口
 * ---------------------------------------------------------------- */

void document_insert_char(Document *doc, int row, int col, char c, bool merge) {
    raw_insert_char(doc, row, col, c);

    /* 判断能否与上条记录合并 */
    if (merge && try_merge(doc, row, col)) {
        undo_append_char(doc, c);
    } else {
        char s[2] = { c, '\0' };
        undo_push(doc, UNDO_INS, row, col, s, 1, false);
    }
}

void document_break_line(Document *doc, int row, int col) {
    raw_break_line(doc, row, col);
    undo_push(doc, UNDO_INS, row, col, "\n", 1, false);
}

void document_delete_char(Document *doc, int row, int col) {
    if (row >= doc->line_count) return;
    Line *line = &doc->lines[row];

    if (col < line->len) {
        /* Phase 5：按 UTF-8 码点删除（1-4 字节），保证不切分多字节序列 */
        int seq = utf8_seq_len((unsigned char)line->text[col]);
        if (col + seq > line->len) seq = line->len - col;  /* 残缺序列降级 */

        char saved[4];
        for (int i = 0; i < seq; i++)
            saved[i] = raw_delete_char(doc, row, col);  /* 每次删 col 处（后续字节上移） */

        undo_push(doc, UNDO_DEL, row, col, saved, seq, false);
    } else if (row + 1 < doc->line_count) {
        /* 行尾 Delete：合并下一行 */
        raw_merge_line(doc, row);
        undo_push(doc, UNDO_DEL, row, col, "\n", 1, false);
    }
}

void document_backspace(Document *doc, int row, int col) {
    if (col > 0) {
        /* Phase 5：向前找码点起始字节，整码点删除 */
        const char *text = doc->lines[row].text;
        int start = utf8_prev_char(text, col);
        int seq   = col - start;

        char saved[4];
        for (int i = 0; i < seq; i++)
            saved[i] = raw_delete_char(doc, row, start);

        undo_push(doc, UNDO_DEL, row, start, saved, seq, false);
    } else if (row > 0) {
        /* 行首 Backspace：合并到上一行 */
        int prev_len = doc->lines[row - 1].len;
        raw_merge_line(doc, row - 1);
        undo_push(doc, UNDO_DEL, row - 1, prev_len, "\n", 1, false);
    }
}

void document_insert_text(Document *doc, int row, int col,
                          const char *text, int len,
                          int *out_row, int *out_col) {
    if (len <= 0) {
        if (out_row) *out_row = row;
        if (out_col) *out_col = col;
        return;
    }
    apply_insert(doc, row, col, text, len, out_row, out_col);
    undo_push(doc, UNDO_INS, row, col, text, len, false);
}

int document_delete_range(Document *doc,
                          int r1, int c1, int r2, int c2,
                          char *out_buf, int out_buf_size) {
    if (r1 > r2 || (r1 == r2 && c1 >= c2)) {
        if (out_buf && out_buf_size > 0) out_buf[0] = '\0';
        return 0;
    }

    /* 计算总字符数（含行间 \n） */
    int total = 0;
    if (r1 == r2) {
        total = c2 - c1;
    } else {
        total = doc->lines[r1].len - c1 + 1; /* +1 for \n */
        for (int r = r1 + 1; r < r2; r++)
            total += doc->lines[r].len + 1;
        total += c2;
    }

    char *deleted = apply_delete(doc, r1, c1, total);
    undo_push(doc, UNDO_DEL, r1, c1, deleted, (int)strlen(deleted), false);

    if (out_buf && out_buf_size > 0) {
        int copy_len = (int)strlen(deleted);
        if (copy_len >= out_buf_size) copy_len = out_buf_size - 1;
        memcpy(out_buf, deleted, (size_t)copy_len);
        out_buf[copy_len] = '\0';
    }
    int ret = (int)strlen(deleted);
    free(deleted);
    return ret;
}

/* ----------------------------------------------------------------
 * 撤销 / 重做
 * ---------------------------------------------------------------- */

int document_undo(Document *doc, int *out_row, int *out_col) {
    if (doc->undo_top == 0) return -1;

    /* 找到本次要撤销的范围（可能有 merged 记录连在一起，但当前实现每条独立） */
    doc->undo_top--;
    UndoRecord *r = &doc->undo_stack[doc->undo_top];

    if (r->type == UNDO_INS) {
        /* 撤销插入 = 删除插入的文本 */
        char *deleted = apply_delete(doc, r->row, r->col, r->len);
        free(deleted);
        if (out_row) *out_row = r->row;
        if (out_col) *out_col = r->col;
    } else {
        /* 撤销删除 = 重新插入被删文本 */
        int new_row, new_col;
        apply_insert(doc, r->row, r->col, r->text, r->len, &new_row, &new_col);
        /* 光标回到删除时的位置 */
        if (r->len == 1 && r->text[0] != '\n') {
            /* 单字符删除：光标回到删除前（col+1） */
            if (out_row) *out_row = r->row;
            if (out_col) *out_col = r->col + 1;
        } else {
            if (out_row) *out_row = new_row;
            if (out_col) *out_col = new_col;
        }
    }

    /* 判断是否回到保存点 */
    if (doc->undo_top == doc->undo_save_idx)
        doc->modified = false;
    else
        doc->modified = true;

    return 0;
}

int document_redo(Document *doc, int *out_row, int *out_col) {
    if (doc->undo_top >= doc->undo_redo_top) return -1;

    UndoRecord *r = &doc->undo_stack[doc->undo_top];
    doc->undo_top++;

    if (r->type == UNDO_INS) {
        int new_row, new_col;
        apply_insert(doc, r->row, r->col, r->text, r->len, &new_row, &new_col);
        if (out_row) *out_row = new_row;
        if (out_col) *out_col = new_col;
    } else {
        char *deleted = apply_delete(doc, r->row, r->col, r->len);
        free(deleted);
        if (out_row) *out_row = r->row;
        if (out_col) *out_col = r->col;
    }

    if (doc->undo_top == doc->undo_save_idx)
        doc->modified = false;
    else
        doc->modified = true;

    return 0;
}

void document_mark_saved(Document *doc) {
    doc->undo_save_idx = doc->undo_top;
    doc->modified = false;
}

/* ----------------------------------------------------------------
 * 查询接口
 * ---------------------------------------------------------------- */

const char* document_get_line(const Document *doc, int row) {
    if (row < 0 || row >= doc->line_count) return "";
    return doc->lines[row].text;
}

int document_get_line_len(const Document *doc, int row) {
    if (row < 0 || row >= doc->line_count) return 0;
    return doc->lines[row].len;
}

int document_line_count(const Document *doc) {
    return doc->line_count;
}

void document_clamp_pos(const Document *doc, int *row, int *col) {
    if (*row < 0) *row = 0;
    if (*row >= doc->line_count) *row = doc->line_count - 1;
    int max_col = doc->lines[*row].len;
    if (*col < 0) *col = 0;
    if (*col > max_col) *col = max_col;
}
