/*
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * phehost: javacall time and timers over POSIX clocks and one thread per timer.
 */
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "javacall_time.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    pthread_t th;
    int ms;
    javacall_bool cyclic;
    javacall_callback_func func;
    volatile int stop;
    volatile int suspended;
} PheTimer;

static void msleep(int ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000; ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static void* timer_thread(void* p) {
    PheTimer* t = (PheTimer*)p;
    do {
        int left = t->ms;
        while (left > 0 && !t->stop) { int s = left > 20 ? 20 : left; msleep(s); left -= s; }
        if (t->stop) break;
        if (!t->suspended) t->func((javacall_handle)t);
    } while (t->cyclic && !t->stop);
    return NULL;
}

javacall_result javacall_time_initialize_timer(int wakeupInMilliSecondsFromNow, javacall_bool cyclic,
                                               javacall_callback_func func, /*OUT*/ javacall_handle* handle) {
    PheTimer* t = (PheTimer*)calloc(1, sizeof(PheTimer));
    if (!t) return JAVACALL_OUT_OF_MEMORY;
    t->ms = wakeupInMilliSecondsFromNow < 1 ? 1 : wakeupInMilliSecondsFromNow;
    t->cyclic = cyclic; t->func = func;
    if (pthread_create(&t->th, NULL, timer_thread, t) != 0) { free(t); return JAVACALL_FAIL; }
    *handle = (javacall_handle)t;
    return JAVACALL_OK;
}

javacall_result javacall_time_finalize_timer(javacall_handle handle) {
    PheTimer* t = (PheTimer*)handle;
    if (!t) return JAVACALL_INVALID_ARGUMENT;
    t->stop = 1;
    if (!pthread_equal(pthread_self(), t->th)) { pthread_join(t->th, NULL); free(t); }
    else pthread_detach(t->th);   /* finalised from its own callback: the thread frees nothing, small leak accepted */
    return JAVACALL_OK;
}

void javacall_time_suspend_ticks(javacall_handle handle) { if (handle) ((PheTimer*)handle)->suspended = 1; }
void javacall_time_resume_ticks(javacall_handle handle) { if (handle) ((PheTimer*)handle)->suspended = 0; }

void javacall_time_sleep(javacall_uint64 ms) { msleep((int)ms); }

char* javacall_time_get_local_timezone(void) { return (char*)"GMT"; }

javacall_int64 javacall_time_get_milliseconds_since_1970(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (javacall_int64)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

javacall_time_seconds javacall_time_get_seconds_since_1970(void) {
    return (javacall_time_seconds)time(NULL);
}

javacall_time_milliseconds javacall_time_get_clock_milliseconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (javacall_time_milliseconds)(ts.tv_sec * 1000UL + ts.tv_nsec / 1000000);
}

javacall_int64 javacall_time_get_monotonic_clock_counter(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (javacall_int64)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

javacall_int64 javacall_time_get_monotonic_clock_frequency(void) { return 1000000000LL; }

#ifdef __cplusplus
}
#endif
