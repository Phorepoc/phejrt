/*
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * phehost: javacall OS primitives over pthreads.
 */
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include "javacall_os.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { pthread_mutex_t m; } PheMutex;
typedef struct { PheMutex* mtx; pthread_cond_t c; } PheCond;

void javacall_os_initialize(void) {}
void javacall_os_dispose(void) {}
void javacall_os_flush_icache(unsigned char* address, int size) { (void)address; (void)size; }

javacall_mutex javacall_os_mutex_create() {
    PheMutex* m = (PheMutex*)malloc(sizeof(PheMutex));
    pthread_mutexattr_t a;
    if (!m) return NULL;
    pthread_mutexattr_init(&a);
    pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&m->m, &a);
    pthread_mutexattr_destroy(&a);
    return (javacall_mutex)m;
}

void javacall_os_mutex_destroy(javacall_mutex mutex) {
    PheMutex* m = (PheMutex*)mutex;
    if (m) { pthread_mutex_destroy(&m->m); free(m); }
}

javacall_result javacall_os_mutex_lock(javacall_mutex mutex) {
    return pthread_mutex_lock(&((PheMutex*)mutex)->m) == 0 ? JAVACALL_OK : JAVACALL_FAIL;
}

javacall_result javacall_os_mutex_try_lock(javacall_mutex mutex) {
    return pthread_mutex_trylock(&((PheMutex*)mutex)->m) == 0 ? JAVACALL_OK : JAVACALL_FAIL;
}

javacall_result javacall_os_mutex_unlock(javacall_mutex mutex) {
    return pthread_mutex_unlock(&((PheMutex*)mutex)->m) == 0 ? JAVACALL_OK : JAVACALL_FAIL;
}

javacall_cond javacall_os_cond_create(javacall_mutex mutex) {
    PheCond* c = (PheCond*)malloc(sizeof(PheCond));
    if (!c) return NULL;
    c->mtx = (PheMutex*)mutex;
    pthread_cond_init(&c->c, NULL);
    return (javacall_cond)c;
}

javacall_mutex javacall_os_cond_get_mutex(javacall_cond cond) {
    return (javacall_mutex)((PheCond*)cond)->mtx;
}

void javacall_os_cond_destroy(javacall_cond cond) {
    PheCond* c = (PheCond*)cond;
    if (c) { pthread_cond_destroy(&c->c); free(c); }
}

javacall_result javacall_os_cond_wait(javacall_cond cond, long millis) {
    PheCond* c = (PheCond*)cond;
    int r;
    if (millis <= 0) {
        r = pthread_cond_wait(&c->c, &c->mtx->m);
    } else {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += millis / 1000;
        ts.tv_nsec += (millis % 1000) * 1000000L;
        if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
        r = pthread_cond_timedwait(&c->c, &c->mtx->m, &ts);
    }
    return r == 0 ? JAVACALL_OK : (r == ETIMEDOUT ? JAVACALL_TIMEOUT : JAVACALL_FAIL);
}

javacall_result javacall_os_cond_signal(javacall_cond cond) {
    return pthread_cond_signal(&((PheCond*)cond)->c) == 0 ? JAVACALL_OK : JAVACALL_FAIL;
}

javacall_result javacall_os_cond_broadcast(javacall_cond cond) {
    return pthread_cond_broadcast(&((PheCond*)cond)->c) == 0 ? JAVACALL_OK : JAVACALL_FAIL;
}

#ifdef __cplusplus
}
#endif
