/*
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * phehost: javacall lifecycle notifications. The host has no application manager: log the state changes.
 */
#include <stdio.h>
#include "javacall_lifecycle.h"

#ifdef __cplusplus
extern "C" {
#endif

javacall_result javacall_lifecycle_platform_request(char* urlString) {
    printf("[phehost] platform request: %s\n", urlString ? urlString : "(null)");
    return JAVACALL_FAIL;
}

void javacall_lifecycle_state_changed(javacall_lifecycle_state state, javacall_result status,
                                      struct javacall_lifecycle_additional_info* additionalInfo) {
    (void)additionalInfo;
    printf("[phehost] lifecycle state %d status %d\n", (int)state, (int)status);
    fflush(stdout);
}

#ifdef __cplusplus
}
#endif
