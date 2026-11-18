/*
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * phehost: the javacall event queue (CLDC flavour): a list of byte buffers guarded by a mutex and a condition
 * variable. javacall_event_send may be called from any thread; javacall_event_receive blocks the Java thread.
 */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include "javacall_events.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Msg { struct Msg* next; int len; unsigned char data[1]; } Msg;

static pthread_mutex_t q_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t q_cond = PTHREAD_COND_INITIALIZER;
static Msg* q_head = NULL;
static Msg* q_tail = NULL;

javacall_result javacall_events_init(void) { return JAVACALL_OK; }
javacall_result javacall_events_finalize(void) { return JAVACALL_OK; }

javacall_result javacall_event_send(unsigned char* binaryBuffer, int binaryBufferLen) {
    Msg* m = (Msg*)malloc(sizeof(Msg) + (size_t)binaryBufferLen);
    if (!m) return JAVACALL_OUT_OF_MEMORY;
    m->next = NULL; m->len = binaryBufferLen;
    memcpy(m->data, binaryBuffer, (size_t)binaryBufferLen);
    pthread_mutex_lock(&q_mtx);
    if (q_tail) q_tail->next = m; else q_head = m;
    q_tail = m;
    pthread_cond_signal(&q_cond);
    pthread_mutex_unlock(&q_mtx);
    return JAVACALL_OK;
}

javacall_result javacall_event_receive(long timeToWaitInMillisec, unsigned char* binaryBuffer,
                                       int binaryBufferMaxLen, /*OUT*/ int* outEventLen) {
    Msg* m;
    javacall_result res = JAVACALL_OK;
    struct timespec ts;
    pthread_mutex_lock(&q_mtx);
    if (timeToWaitInMillisec > 0) {
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeToWaitInMillisec / 1000;
        ts.tv_nsec += (timeToWaitInMillisec % 1000) * 1000000L;
        if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
    }
    while (!q_head) {
        if (timeToWaitInMillisec == 0) break;
        if (timeToWaitInMillisec < 0) pthread_cond_wait(&q_cond, &q_mtx);
        else if (pthread_cond_timedwait(&q_cond, &q_mtx, &ts) == ETIMEDOUT) break;
    }
    m = q_head;
    if (m) { q_head = m->next; if (!q_head) q_tail = NULL; }
    pthread_mutex_unlock(&q_mtx);
    if (!m) { *outEventLen = 0; return JAVACALL_TIMEOUT; }
    if (m->len > binaryBufferMaxLen) { res = JAVACALL_OUT_OF_MEMORY; *outEventLen = binaryBufferMaxLen; }
    else *outEventLen = m->len;
    memcpy(binaryBuffer, m->data, (size_t)*outEventLen);
    free(m);
    return res;
}

/* CDC flavour: not used by the CLDC build. */
javacall_result javacall_event_receive_cvm(int queueId, unsigned char* binaryBuffer, int binaryBufferMaxLen, /*OUT*/ int* outEventLen) {
    (void)queueId;
    return javacall_event_receive(-1, binaryBuffer, binaryBufferMaxLen, outEventLen);
}

javacall_result javacall_event_send_cvm(int queueId, unsigned char* binaryBuffer, int binaryBufferLen) {
    (void)queueId;
    return javacall_event_send(binaryBuffer, binaryBufferLen);
}

#ifdef __cplusplus
}
#endif
