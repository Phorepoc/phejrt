/*
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * phehost on Symbian: the one place that calls the native OS. phe_rdebug sends text to the debug port (serial in an
 * emulator, RDebug on a device) a line at a time, as Phorepoc's boot tests do. The VM prints a character or a fragment
 * per call, so a partial line is kept until its newline.
 */
#ifdef __SYMBIAN32__
#include <e32std.h>
#include <e32debug.h>

static TBuf8<200> gLine;

static void flushLine() {
    if (gLine.Length()) {
        gLine.Append(_L8("\r\n"));          // RawPrint adds no newline
        RDebug::RawPrint(gLine);
        gLine.Zero();
    }
}

extern "C" void phe_rdebug(const char* s, int length) {
    for (int i = 0; i < length; i++) {
        char c = s[i];
        if (c == '\n') {
            flushLine();
        } else if (c != '\r') {
            if (gLine.Length() >= 190) flushLine();
            gLine.Append((TUint8)c);
        }
    }
}
#endif
