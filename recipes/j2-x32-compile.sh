#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: GPL-2.0-only
#
# j2-x32-compile.sh -- step J2 (x32 part, compile-only): compile PCSL, our phehost javacall port and every MIDP C source
# for the x32 ABI (-mx32: amd64 CPU, 32-bit pointers, ILP32 types), the ABI Phorepoc uses, to find 64-bit-isms before the
# Symbian-side build. Run after recipes/j1-host.sh build (same J1_WORK). Needs libc6-dev-x32. An x32 binary cannot run in
# Docker Desktop's kernel, so there is no link step here: the objects are counted, and the first errors printed.
# Output: $J1_WORK/j2x32.log, objects under $J1_WORK/out/javacall_x32.
set -eu
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd); PMS=$(dirname "$HERE")
W=${J1_WORK:-/work}; PM=$W/pm; OUT=$W/out; MX=$W/mx32
JDK=${JDK_DIR:-/usr/lib/jvm/java-8-openjdk-amd64}
JC=$OUT/jc_x32; X=$OUT/javacall_x32
LOG=$W/j2x32.log; : > "$LOG"
say() { echo "== $*" | tee -a "$LOG"; }

mkdir -p "$MX"
for t in gcc g++; do
    if [ $t = gcc ]; then F='-fcommon -std=gnu89'; else F='-fpermissive -std=gnu++98'; fi
    printf '#!/bin/bash\nargs=(); for a in "$@"; do case "$a" in -static|-m32) ;; *) args+=("$a");; esac; done\nexec /usr/bin/%s -mx32 %s "${args[@]}" -w -Wno-error\n' "$t" "$F" > "$MX/$t"
    chmod +x "$MX/$t"
done
export PATH=$MX:$PATH GCC_LIBS="-lpthread -lm"

say "X1 phehost javacall library for x32"
rm -rf "$W/phehost_x32" "$JC"; mkdir -p "$JC"; cp -r "$PMS/javacall/phehost" "$W/phehost_x32"
( cd "$W/phehost_x32/config" && make JAVACALL_DIR="$PM/javacall" TOOLS_DIR="$PM/tools" PHEHOST_DIR="$W/phehost_x32" \
    JAVACALL_OUTPUT_DIR="$JC" USE_STATIC_PROPERTIES=true JDK_DIR="$JDK" >> "$LOG" 2>&1 ) || say "javacall x32 library: FAILED (see log)"
say "X2 PCSL javacall_i386_gcc for x32"
rm -rf "$X/pcsl"; mkdir -p "$X/pcsl"
export JAVACALL_LIB_DIR=$JC/lib JAVACALL_OUTPUT_DIR=$JC JAVACALL_DIR=$PM/javacall
make -C "$PM/pcsl" PCSL_PLATFORM=javacall_i386_gcc PCSL_OUTPUT_DIR="$X/pcsl" JDK_DIR="$JDK" >> "$LOG" 2>&1 || true
say "X3 MIDP C sources for x32 (make -k; the link and the Java steps are expected to fail or be skipped)"
mkdir -p "$X"
make -C "$PM/midp/build/javacall" -k JDK_DIR="$JDK" TOOLS_DIR="$PM/tools" CLDC_DIR="$PM/cldc" PCSL_DIR="$PM/pcsl" \
    JAVACALL_DIR="$PM/javacall" MIDP_OUTPUT_DIR="$X" PCSL_OUTPUT_DIR="$X/pcsl" \
    CLDC_DIST_DIR="$OUT/cldc_jc/javacall_i386_gcc/dist" USE_JSR_293=false JAVACALL_PLATFORM=linux_i386_gcc \
    JAVACALL_OUTPUT_DIR="$JC" USE_STATIC_PROPERTIES=true USE_DYNAMIC_PERMISSIONS=false >> "$LOG" 2>&1 || true
N=$(find "$X/obj" -name '*.o' 2>/dev/null | wc -l)
E=$(grep -c ' error: ' "$LOG" || true)
say "x32 objects: $N, compile errors: $E"
file "$(find "$X/obj" -name '*.o' | head -1)" | tee -a "$LOG"
grep ' error: ' "$LOG" | head -20
