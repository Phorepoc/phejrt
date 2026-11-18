/*
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * phehost on Symbian: the one place that calls the native OS. phe_rdebug sends text to the debug port (serial in an
 * emulator, RDebug on a device) a line at a time, as Phorepoc's boot tests do.
 */
#ifdef __SYMBIAN32__
#include <e32std.h>
#include <e32debug.h>

extern "C" void phe_rdebug(const char* s, int length) {
    TBuf8<200> line;
    for (int i = 0; i < length; i++) {
        char c = s[i];
        if (c == '\n' || line.Length() >= 190) {
            if (line.Length() || c == '\n') { line.Append(_L8("\r\n")); RDebug::RawPrint(line); }   // RawPrint adds no newline
            line.Zero();
            if (c == '\n') continue;
        }
        if (c != '\r') line.Append((TUint8)c);
    }
    if (line.Length()) { line.Append(_L8("\r\n")); RDebug::RawPrint(line); }
}
#endif
