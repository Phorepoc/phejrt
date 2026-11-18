/*
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * phehost: javacall logging to stdout (the stubs implementation printed nothing).
 */
#include <stdio.h>
#include "javacall_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

void javacall_print(const char* s) {
    fputs(s, stdout);
    fflush(stdout);
}

void javacall_print_chars(const char* s, int length) {
    fwrite(s, 1, (size_t)length, stdout);
    fflush(stdout);
}

const char* javacall_jlong_format_specifier() {
    return "lld";
}

const char* javacall_julong_format_specifier() {
    return "llu";
}

#ifdef __cplusplus
}
#endif
