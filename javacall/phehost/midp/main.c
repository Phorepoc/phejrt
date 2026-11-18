/*
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * phehost: the host executable's main(). The Java task (JavaTask: VM + MIDP, from midp/src/ams) runs on its own
 * thread and receives its work as javacall events; main() posts the command line as a "start with arbitrary
 * arguments" event (the argument layout of midp's runMidlet) and waits for the Java task to end.
 */
#include <stdio.h>
#include <pthread.h>
#include "javacall_properties.h"
#include "javacall_lifecycle.h"
#include "javacall_events.h"
#include "javacall_lcd.h"

extern void JavaTask(void);
extern void javanotify_start_java_with_arbitrary_args(int argc, char* argv[]);

static void* java_thread(void* arg) {
    (void)arg;
    javacall_lcd_init();
    JavaTask();
    return NULL;
}

#ifdef __SYMBIAN32__
/* An EXE started by the system has no command line: run the test MIDlet that ships in the ROM (Z:\phejava). */
static char* default_argv[] = { "runMidlet", "-1", "HelloJ1", "-classpathext", "/z/phejava/hello.jar", NULL };
#endif

int main(int argc, char* argv[]) {
    pthread_t t;
    pthread_attr_t attr;
#ifdef __SYMBIAN32__
    if (argc <= 1) { argc = 5; argv = default_argv; }
#endif
    javacall_initialize_configurations();
    javacall_events_init();
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 1024 * 1024);   /* the C interpreter recurses on the native stack */
    if (pthread_create(&t, &attr, java_thread, NULL) != 0) {
        fputs("phehost: cannot start the Java thread\n", stderr);
        return 2;
    }
    javanotify_start_java_with_arbitrary_args(argc, argv);
    pthread_join(t, NULL);
    javacall_events_finalize();
    return 0;
}
