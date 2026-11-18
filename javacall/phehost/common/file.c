/*
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * phehost: javacall file API over POSIX. Names arrive as UTF-16 and are converted to UTF-8.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include "javacall_file.h"
#include "phehost_util.h"

#ifdef __cplusplus
extern "C" {
#endif

int phe_utf16_to_path(const javacall_utf16* in, int len, char* out, int outSize) {
    int i = 0, o = 0;
    if (len < 0) { len = 0; while (in[len]) len++; }
    while (i < len) {
        unsigned long c = in[i++];
        if (c >= 0xD800 && c < 0xDC00 && i < len && in[i] >= 0xDC00 && in[i] < 0xE000) {
            c = 0x10000 + ((c - 0xD800) << 10) + (in[i++] - 0xDC00);
        }
        if (o + 5 >= outSize) return -1;
        if (c < 0x80) { out[o++] = (char)c; }
        else if (c < 0x800) { out[o++] = (char)(0xC0 | (c >> 6)); out[o++] = (char)(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) { out[o++] = (char)(0xE0 | (c >> 12)); out[o++] = (char)(0x80 | ((c >> 6) & 0x3F)); out[o++] = (char)(0x80 | (c & 0x3F)); }
        else { out[o++] = (char)(0xF0 | (c >> 18)); out[o++] = (char)(0x80 | ((c >> 12) & 0x3F)); out[o++] = (char)(0x80 | ((c >> 6) & 0x3F)); out[o++] = (char)(0x80 | (c & 0x3F)); }
    }
    out[o] = 0;
    return o;
}

int phe_utf8_to_utf16(const char* in, javacall_utf16* out, int outSize) {
    int o = 0;
    const unsigned char* p = (const unsigned char*)in;
    while (*p) {
        unsigned long c = *p++;
        int n = 0;
        if (c >= 0xF0) { c &= 0x07; n = 3; } else if (c >= 0xE0) { c &= 0x0F; n = 2; } else if (c >= 0xC0) { c &= 0x1F; n = 1; }
        while (n-- > 0 && (*p & 0xC0) == 0x80) c = (c << 6) | (*p++ & 0x3F);
        if (c >= 0x10000) {
            if (o + 2 >= outSize) return -1;
            c -= 0x10000;
            out[o++] = (javacall_utf16)(0xD800 + (c >> 10));
            out[o++] = (javacall_utf16)(0xDC00 + (c & 0x3FF));
        } else {
            if (o + 1 >= outSize) return -1;
            out[o++] = (javacall_utf16)c;
        }
    }
    out[o] = 0;
    return o;
}

javacall_result javacall_file_init(void) { return JAVACALL_OK; }
javacall_result javacall_file_finalize(void) { return JAVACALL_OK; }

javacall_result javacall_file_open(const javacall_utf16* name, int nameLen, int flags, /*OUT*/ javacall_handle* handle) {
    char path[PHE_MAX_PATH];
    int f, fd;
    if (phe_utf16_to_path(name, nameLen, path, sizeof(path)) < 0) return JAVACALL_FAIL;
    if (flags & JAVACALL_FILE_O_RDWR) f = O_RDWR;
    else if (flags & JAVACALL_FILE_O_WRONLY) f = O_WRONLY;
    else f = O_RDONLY;
    if (flags & JAVACALL_FILE_O_CREAT) f |= O_CREAT;
    if (flags & JAVACALL_FILE_O_TRUNC) f |= O_TRUNC;
    if (flags & JAVACALL_FILE_O_APPEND) f |= O_APPEND;
    fd = open(path, f, 0666);
    if (fd < 0) { *handle = NULL; return JAVACALL_FAIL; }
    *handle = (javacall_handle)(long)(fd + 1);   /* 0 is not a valid handle */
    return JAVACALL_OK;
}

#define FD(h) ((int)(long)(h) - 1)

javacall_result javacall_file_close(javacall_handle handle) {
    return close(FD(handle)) == 0 ? JAVACALL_OK : JAVACALL_FAIL;
}

long javacall_file_read(javacall_handle handle, unsigned char* buf, long size) {
    ssize_t n = read(FD(handle), buf, (size_t)size);
    return n < 0 ? -1 : (long)n;
}

long javacall_file_write(javacall_handle handle, const unsigned char* buf, long size) {
    ssize_t n = write(FD(handle), buf, (size_t)size);
    return n < 0 ? -1 : (long)n;
}

javacall_result javacall_file_delete(const javacall_utf16* name, int nameLen) {
    char path[PHE_MAX_PATH];
    if (phe_utf16_to_path(name, nameLen, path, sizeof(path)) < 0) return JAVACALL_FAIL;
    return unlink(path) == 0 ? JAVACALL_OK : JAVACALL_FAIL;
}

javacall_result javacall_file_truncate(javacall_handle handle, javacall_int64 size) {
    return ftruncate(FD(handle), (off_t)size) == 0 ? JAVACALL_OK : JAVACALL_FAIL;
}

javacall_int64 javacall_file_seek(javacall_handle handle, javacall_int64 offset, javacall_file_seek_flags flag) {
    int w = flag == JAVACALL_FILE_SEEK_CUR ? SEEK_CUR : flag == JAVACALL_FILE_SEEK_END ? SEEK_END : SEEK_SET;
    off_t r = lseek(FD(handle), (off_t)offset, w);
    return r < 0 ? -1 : (javacall_int64)r;
}

javacall_int64 javacall_file_sizeofopenfile(javacall_handle handle) {
    struct stat st;
    return fstat(FD(handle), &st) == 0 ? (javacall_int64)st.st_size : -1;
}

javacall_int64 javacall_file_sizeof(const javacall_utf16* name, int nameLen) {
    char path[PHE_MAX_PATH];
    struct stat st;
    if (phe_utf16_to_path(name, nameLen, path, sizeof(path)) < 0) return -1;
    return stat(path, &st) == 0 ? (javacall_int64)st.st_size : -1;
}

javacall_result javacall_file_exist(const javacall_utf16* name, int nameLen) {
    char path[PHE_MAX_PATH];
    struct stat st;
    if (phe_utf16_to_path(name, nameLen, path, sizeof(path)) < 0) return JAVACALL_FAIL;
    return stat(path, &st) == 0 ? JAVACALL_OK : JAVACALL_FAIL;
}

javacall_result javacall_file_flush(javacall_handle handle) {
    return fsync(FD(handle)) == 0 ? JAVACALL_OK : JAVACALL_FAIL;
}

javacall_result javacall_file_rename(const javacall_utf16* oldName, int oldLen, const javacall_utf16* newName, int newLen) {
    char a[PHE_MAX_PATH], b[PHE_MAX_PATH];
    if (phe_utf16_to_path(oldName, oldLen, a, sizeof(a)) < 0) return JAVACALL_FAIL;
    if (phe_utf16_to_path(newName, newLen, b, sizeof(b)) < 0) return JAVACALL_FAIL;
    return rename(a, b) == 0 ? JAVACALL_OK : JAVACALL_FAIL;
}

#ifdef __cplusplus
}
#endif
