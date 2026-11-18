/*
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * phehost: javacall logging to stdout (the stubs implementation printed nothing).
 */
#include <stdio.h>
#include <string.h>
#include "javacall_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __SYMBIAN32__
/* Symbian: the debug port (serial in an emulator, RDebug on a device); see phe_symbian.cpp. */
extern void phe_rdebug(const char* s, int length);
void javacall_print(const char* s) {
    phe_rdebug(s, (int)strlen(s));
}

void javacall_print_chars(const char* s, int length) {
    phe_rdebug(s, length);
}
#else
void javacall_print(const char* s) {
    fputs(s, stdout);
    fflush(stdout);
}

void javacall_print_chars(const char* s, int length) {
    fwrite(s, 1, (size_t)length, stdout);
    fflush(stdout);
}
#endif

const char* javacall_jlong_format_specifier() {
    return "lld";
}

const char* javacall_julong_format_specifier() {
    return "llu";
}

#ifdef __cplusplus
}
#endif
