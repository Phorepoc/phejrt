#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: GPL-2.0-only
#
# j2-host.sh -- step J2 (host part): our headless javacall port (javacall/phehost) in place of the stub library, then
# relink midp's runMidlet against it and run HelloJ1. Run after recipes/j1-host.sh build (same J1_WORK): that script
# leaves the patched phoneME tree, PCSL, CLDC (javacall flavour) and the midp objects this one reuses.
set -eu
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd); PMS=$(dirname "$HERE")
W=${J1_WORK:-/work}; PM=$W/pm; OUT=$W/out; M32=$W/m32
JDK=${JDK_DIR:-/usr/lib/jvm/java-8-openjdk-amd64}
JC=$OUT/jc2; J=$OUT/javacall
export PATH=$M32:$PATH GCC_LIBS="-lpthread -lm"
LOG=$W/j2.log; : > "$LOG"
say() { echo "== $*" | tee -a "$LOG"; }

say "J2a javacall library from javacall/phehost"
rm -rf "$JC"; mkdir -p "$JC"
rm -rf "$W/phehost"; cp -r "$PMS/javacall/phehost" "$W/phehost"
( cd "$W/phehost/config" && make JAVACALL_DIR="$PM/javacall" TOOLS_DIR="$PM/tools" PHEHOST_DIR="$W/phehost" \
    JAVACALL_OUTPUT_DIR="$JC" USE_STATIC_PROPERTIES=true JDK_DIR="$JDK" >> "$LOG" 2>&1 ) || { tail -20 "$LOG"; exit 1; }

say "J2b relink midp runMidlet against it"
export EXTRA_LD="-L$JC/lib -ljavacall -lpthread" JAVACALL_LIB_DIR=$JC/lib JAVACALL_OUTPUT_DIR=$JC JAVACALL_DIR=$PM/javacall
rm -f "$J/bin/i386/runMidlet"
make -C "$PM/midp/build/javacall" JDK_DIR="$JDK" TOOLS_DIR="$PM/tools" CLDC_DIR="$PM/cldc" PCSL_DIR="$PM/pcsl" \
    JAVACALL_DIR="$PM/javacall" MIDP_OUTPUT_DIR="$J" PCSL_OUTPUT_DIR="$J/pcsl" \
    CLDC_DIST_DIR="$OUT/cldc_jc/javacall_i386_gcc/dist" USE_JSR_293=false JAVACALL_PLATFORM=linux_i386_gcc \
    JAVACALL_OUTPUT_DIR="$JC" USE_STATIC_PROPERTIES=true USE_DYNAMIC_PERMISSIONS=false -k >> "$LOG" 2>&1 || true
test -x "$J/bin/i386/runMidlet" || { tail -20 "$LOG"; exit 1; }

say "J2c run HelloJ1 on the host"
rm -rf "$J/phehome" "$J/phecfg"; mkdir -p "$J/phehome" "$J/phecfg"; cp -r "$J"/appdb/. "$J"/lib/. "$J/phecfg/"
export PHE_JAVA_HOME=$J/phehome PHE_JAVA_CONFIG=$J/phecfg PHE_LCD_DUMP=$W/j1/lcd.ppm
( cd "$J/bin/i386" && timeout -s KILL "${J2_TIMEOUT:-30}" ./runMidlet -1 HelloJ1 -classpathext "$W/j1/hello.jar" ) > "$W/j1/run-phehost.log" 2>&1 || say "runMidlet exit $?"
grep -a '\[J1\]\|\[phehost\]' "$W/j1/run-phehost.log" | tee -a "$LOG" || true
