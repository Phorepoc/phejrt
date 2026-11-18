/*
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: GPL-2.0-only
 */
#ifndef PHEHOST_UTIL_H
#define PHEHOST_UTIL_H

#include "javacall_defs.h"

#define PHE_MAX_PATH 1024

/* UTF-16 (len code units, or -1 = NUL-terminated) -> UTF-8 into out; returns length or -1. */
int phe_utf16_to_path(const javacall_utf16* in, int len, char* out, int outSize);
/* UTF-8 -> UTF-16; returns number of code units or -1. */
int phe_utf8_to_utf16(const char* in, javacall_utf16* out, int outSize);

#endif
