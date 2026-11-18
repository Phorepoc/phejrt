/*
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * phehost: javacall directory API over POSIX. The Java home is $PHE_JAVA_HOME (default ./Java), the configuration
 * directory (appdb: keystore, skin images) is $PHE_JAVA_CONFIG (default $PHE_JAVA_HOME/appdb).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#ifndef __SYMBIAN32__
#include <sys/statvfs.h>
#endif
#include "javacall_dir.h"

/* Symbian (Open C): the writable Java home is on C:, the configuration (skin.bin, keystore) ships on Z:. */
#ifdef __SYMBIAN32__
#define PHE_DEFAULT_HOME   "/c/phejava"
#define PHE_DEFAULT_CONFIG "/z/phejava/cfg"
#else
#define PHE_DEFAULT_HOME   "./Java"
#define PHE_DEFAULT_CONFIG NULL
#endif
#include "phehost_util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { DIR* d; javacall_utf16 name[PHE_MAX_PATH]; } PheDir;

javacall_handle javacall_dir_open(const javacall_utf16* path, int pathLen) {
    char p[PHE_MAX_PATH];
    PheDir* h;
    if (phe_utf16_to_path(path, pathLen, p, sizeof(p)) < 0) return NULL;
    h = (PheDir*)malloc(sizeof(PheDir));
    if (!h) return NULL;
    h->d = opendir(p);
    if (!h->d) { free(h); return NULL; }
    return (javacall_handle)h;
}

void javacall_dir_close(javacall_handle handle) {
    PheDir* h = (PheDir*)handle;
    if (h) { closedir(h->d); free(h); }
}

javacall_utf16* javacall_dir_get_next(javacall_handle handle, int* /*OUT*/ outFilenameLength) {
    PheDir* h = (PheDir*)handle;
    struct dirent* e;
    while ((e = readdir(h->d)) != NULL) {
        int n;
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        n = phe_utf8_to_utf16(e->d_name, h->name, PHE_MAX_PATH);
        if (n < 0) continue;
        *outFilenameLength = n;
        return h->name;
    }
    return NULL;
}

javacall_int64 javacall_dir_get_free_space_for_java(void) {
#ifdef __SYMBIAN32__
    return (javacall_int64)32 * 1024 * 1024;   /* Open C has no statvfs: a fixed budget for the Java home */
#else
    struct statvfs s;
    const char* r = getenv("PHE_JAVA_HOME");
    if (statvfs(r ? r : ".", &s) != 0) return 0;
    return (javacall_int64)s.f_bavail * (javacall_int64)s.f_frsize;
#endif
}

static javacall_result put(const char* utf8, javacall_utf16* out, int* outLen) {
    javacall_utf16 tmp[PHE_MAX_PATH];
    int n = phe_utf8_to_utf16(utf8, tmp, PHE_MAX_PATH);
    if (n < 0 || n >= *outLen) return JAVACALL_FAIL;
    memcpy(out, tmp, (size_t)(n + 1) * sizeof(javacall_utf16));
    *outLen = n;
    return JAVACALL_OK;
}

javacall_result javacall_dir_get_root_path(javacall_utf16* /* OUT */ rootPath, int* /* IN | OUT */ rootPathLen) {
    const char* r = getenv("PHE_JAVA_HOME");
    char buf[PHE_MAX_PATH];
    snprintf(buf, sizeof(buf), "%s", r ? r : PHE_DEFAULT_HOME);
    mkdir(buf, 0777);
    return put(buf, rootPath, rootPathLen);
}

javacall_result javacall_dir_get_config_path(javacall_utf16* /* OUT */ configPath, int* /* IN | OUT */ configPathLen) {
    const char* c = getenv("PHE_JAVA_CONFIG");
    const char* r = getenv("PHE_JAVA_HOME");
    char buf[PHE_MAX_PATH];
    if (!c) c = PHE_DEFAULT_CONFIG;
    if (c) snprintf(buf, sizeof(buf), "%s", c);
    else snprintf(buf, sizeof(buf), "%s/appdb", r ? r : PHE_DEFAULT_HOME);
    mkdir(buf, 0777);
    return put(buf, configPath, configPathLen);
}

javacall_utf16 javacall_get_file_separator(void) {
    return '/';
}

javacall_bool javacall_dir_is_secure_storage(javacall_utf16* classPath, int pathLen) {
    (void)classPath; (void)pathLen;
    return JAVACALL_FALSE;
}

#ifdef __cplusplus
}
#endif
