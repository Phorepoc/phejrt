/*
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * phehost: the javacall event-queue lock (one recursive process-wide mutex).
 */
#include <pthread.h>
#include "javacall_eventqueue.h"

#ifdef __cplusplus
extern "C" {
#endif

static pthread_mutex_t lock;
static int lock_ready;

javacall_result javacall_create_event_queue_lock(void) {
    pthread_mutexattr_t a;
    if (lock_ready) return JAVACALL_OK;
    pthread_mutexattr_init(&a);
    pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&lock, &a);
    pthread_mutexattr_destroy(&a);
    lock_ready = 1;
    return JAVACALL_OK;
}

javacall_result javacall_destroy_event_queue_lock(void) {
    if (lock_ready) { pthread_mutex_destroy(&lock); lock_ready = 0; }
    return JAVACALL_OK;
}

javacall_result javacall_wait_and_lock_event_queue(void) {
    if (!lock_ready) javacall_create_event_queue_lock();
    return pthread_mutex_lock(&lock) == 0 ? JAVACALL_OK : JAVACALL_FAIL;
}

javacall_result javacall_unlock_event_queue(void) {
    return pthread_mutex_unlock(&lock) == 0 ? JAVACALL_OK : JAVACALL_FAIL;
}

#ifdef __cplusplus
}
#endif
