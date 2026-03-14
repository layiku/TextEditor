/*
 * util.c — 通用工具函数实现
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "util.h"

#ifdef _WIN32
#  include <windows.h>
#  include <shlobj.h>
/* strncasecmp 在 Windows 下叫 _strnicmp */
#  define strncasecmp _strnicmp
#else
#  include <unistd.h>
#  include <sys/stat.h>
#  include <limits.h>
#  include <dirent.h>
#  if defined(__linux__) || defined(__unix__)
#    include <pwd.h>
#  endif
#endif

/* ================================================================
 * 内存封装
 * ================================================================ */

void* safe_malloc(size_t size) {
    void *p = malloc(size);
    if (!p) {
        fprintf(stderr, "Fatal: malloc(%zu) OOM\n", size);
        exit(1);
    }
    return p;
}

void* safe_realloc(void *ptr, size_t size) {
    void *p = realloc(ptr, size);
    if (!p && size > 0) {
        fprintf(stderr, "Fatal: realloc(%zu) OOM\n", size);
        exit(1);
    }
    return p;
}

char* safe_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *d = (char*)safe_malloc(len + 1);
    memcpy(d, s, len + 1);
    return d;
}

/* ================================================================
 * 字符串工具
 * ================================================================ */

void str_trim_crlf(char *s) {
    int n = (int)strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r'))
        s[--n] = '\0';
}

int str_starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

int str_icmp(const char *a, const char *b) {
    while (*a && *b) {
        int diff = tolower((unsigned char)*a) - tolower((unsigned char)*b);
        if (diff != 0) return diff;
        a++; b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

const char* str_istr(const char *haystack, const char *needle) {
    if (!needle || !*needle) return haystack;
    size_t nlen = strlen(needle);
    for (; *haystack; haystack++) {
        if (strncasecmp(haystack, needle, nlen) == 0)
            return haystack;
    }
    return NULL;
}

/* ================================================================
 * 路径工具
 * ================================================================ */

bool file_exists(const char *path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES) &&
           !(attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return (stat(path, &st) == 0) && S_ISREG(st.st_mode);
#endif
}

void path_get_dir(const char *path, char *out, int out_size) {
    strncpy(out, path, (size_t)(out_size - 1));
    out[out_size - 1] = '\0';
    /* 从末尾向前找第一个路径分隔符 */
    char *sep = NULL;
    for (char *p = out; *p; p++) {
        if (*p == '/' || *p == '\\') sep = p;
    }
    if (sep && sep > out) {
        *sep = '\0';
    } else if (sep == out) {
        /* 根目录如 "/foo" 情况 */
        *(sep + 1) = '\0';
    } else {
        /* 没有分隔符，返回当前目录 */
        out[0] = '.'; out[1] = '\0';
    }
}

void path_join(const char *dir, const char *name, char *out, int out_size) {
    int n = snprintf(out, (size_t)out_size, "%s", dir);
    if (n > 0 && n < out_size - 1) {
        char last = out[n - 1];
        if (last != '/' && last != '\\')
            snprintf(out + n, (size_t)(out_size - n), "/%s", name);
        else
            snprintf(out + n, (size_t)(out_size - n), "%s", name);
    }
}

void get_exe_dir(char *out, int out_size) {
#ifdef _WIN32
    char buf[MAX_PATH];
    GetModuleFileNameA(NULL, buf, MAX_PATH);
    path_get_dir(buf, out, out_size);
#elif defined(__linux__)
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        path_get_dir(buf, out, out_size);
    } else {
        strncpy(out, ".", (size_t)(out_size - 1));
        out[out_size - 1] = '\0';
    }
#else
    strncpy(out, ".", (size_t)(out_size - 1));
    out[out_size - 1] = '\0';
#endif
}

void get_user_config_dir(char *out, int out_size) {
#ifdef _WIN32
    char buf[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, buf))) {
        strncpy(out, buf, (size_t)(out_size - 1));
    } else {
        strncpy(out, ".", (size_t)(out_size - 1));
    }
    out[out_size - 1] = '\0';
#else
    const char *home = getenv("HOME");
    if (!home) {
#  if defined(__linux__) || defined(__unix__)
        struct passwd *pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : ".";
#  else
        home = ".";
#  endif
    }
    strncpy(out, home, (size_t)(out_size - 1));
    out[out_size - 1] = '\0';
#endif
}

/* ================================================================
 * 目录枚举
 * ================================================================ */

bool path_is_dir(const char *path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES) &&
           (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return (stat(path, &st) == 0) && S_ISDIR(st.st_mode);
#endif
}

/* qsort 比较：目录在前，同类按名称升序 */
static int cmp_dir_entry(const void *a, const void *b) {
    const DirEntry *ea = (const DirEntry *)a;
    const DirEntry *eb = (const DirEntry *)b;
    /* ".." 固定最前 */
    if (strcmp(ea->name, "..") == 0) return -1;
    if (strcmp(eb->name, "..") == 0) return  1;
    /* 目录在文件之前 */
    if (ea->is_dir != eb->is_dir)
        return ea->is_dir ? -1 : 1;
    return strcmp(ea->name, eb->name);
}

int dir_read_entries(const char *dir, DirEntry *out, int max_count) {
    if (!dir || !dir[0] || !out || max_count <= 0) return 0;
    int count = 0;

#ifdef _WIN32
    char pattern[600];
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;

    do {
        /* 跳过 "."（当前目录），保留 ".." */
        if (strcmp(fd.cFileName, ".") == 0) continue;
        if (count >= max_count) break;
        strncpy(out[count].name, fd.cFileName,
                DIR_ENTRY_MAX_NAME - 1);
        out[count].name[DIR_ENTRY_MAX_NAME - 1] = '\0';
        out[count].is_dir = (fd.dwFileAttributes &
                              FILE_ATTRIBUTE_DIRECTORY) != 0;
        count++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);

#else
    DIR *dp = opendir(dir);
    if (!dp) return 0;
    struct dirent *ent;
    while ((ent = readdir(dp)) != NULL && count < max_count) {
        if (strcmp(ent->d_name, ".") == 0) continue;
        strncpy(out[count].name, ent->d_name, DIR_ENTRY_MAX_NAME - 1);
        out[count].name[DIR_ENTRY_MAX_NAME - 1] = '\0';
        /* 构造完整路径以判断类型 */
        char full[600];
        snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);
        out[count].is_dir = path_is_dir(full);
        count++;
    }
    closedir(dp);
#endif

    qsort(out, (size_t)count, sizeof(DirEntry), cmp_dir_entry);
    return count;
}
