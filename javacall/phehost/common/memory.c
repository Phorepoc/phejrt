/*
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * phehost: javacall memory over libc (the stubs implementation returned NULL).
 */
#include <stdlib.h>
#include "javacall_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

void* javacall_memory_heap_allocate(long size, /*OUT*/ long* outSize) {
    void* p = malloc((size_t)size);
    *outSize = p ? size : 0;
    return p;
}

void javacall_memory_heap_deallocate(void* heap) {
    free(heap);
}

void* javacall_malloc(unsigned int size) {
    return malloc(size);
}

void* javacall_realloc(void* ptr, unsigned int size) {
    return realloc(ptr, size);
}

void javacall_free(void* ptr) {
    free(ptr);
}

#ifdef __cplusplus
}
#endif
