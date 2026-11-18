/*
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * phehost: javacall string helpers (ASCII/Latin-1 case mapping; full Unicode tables are a later step).
 */
#include "javacall_string.h"

#ifdef __cplusplus
extern "C" {
#endif

javacall_result javacall_towupper(javacall_utf16* chars, unsigned int count) {
    unsigned int i;
    for (i = 0; i < count; i++) {
        javacall_utf16 c = chars[i];
        if ((c >= 'a' && c <= 'z') || (c >= 0xE0 && c <= 0xFE && c != 0xF7)) {
            chars[i] = (javacall_utf16)(c - 32);
        }
    }
    return JAVACALL_OK;
}

javacall_result javacall_towlower(javacall_utf16* chars, unsigned int count) {
    unsigned int i;
    for (i = 0; i < count; i++) {
        javacall_utf16 c = chars[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7)) {
            chars[i] = (javacall_utf16)(c + 32);
        }
    }
    return JAVACALL_OK;
}

#ifdef __cplusplus
}
#endif
