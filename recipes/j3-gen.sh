#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: GPL-2.0-only
#
# j3-gen.sh -- step J3, host half: build PCSL, the C-interpreter CLDC-HI VM (cldc/build/javacall_c) and MIDP for the
# javacall platform with our phehost port, record the compiler commands, and write the Symbian .mmp
# (gen_mmp_midp.py) into $J1_WORK/j3. Run after recipes/j1-host.sh build and recipes/j2-host.sh (same J1_WORK); the
# result is also a host regression: the C-interpreter runMidlet runs HelloJ1 here before anything is built for Symbian.
#
# The x32 half (in the phorepoc-x86 container, with the private repo's emitter):
#   PHE_J3_WORK=$J1_WORK MMP_JOBS=4 python3 emitter/build_mmp_x32.py phemidp      (key: emitter/components_phemidp.py)
# $J1_WORK must be visible at the same path in that container (a volume): the generated sources include each other by
# absolute path. MMP_JOBS=4: the merged VM sources are big (24 parallel compiles exceeded 6 GB).
set -eu
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd); PMS=$(dirname "$HERE")
W=${J1_WORK:-/work}; PM=$W/pm; OUT=$W/out; M32=$W/m32
JDK=${JDK_DIR:-/usr/lib/jvm/java-8-openjdk-amd64}
JC=$OUT/jc2
export PATH=$M32:$PATH GCC_LIBS="-lpthread -lm"
export EXTRA_LD="-L$JC/lib -ljavacall -lpthread" JAVACALL_LIB_DIR=$JC/lib JAVACALL_OUTPUT_DIR=$JC JAVACALL_DIR=$PM/javacall
say() { echo "== $*"; }

say "G1 PCSL (javacall flavour)"
rm -rf "$OUT/pcsl3"; mkdir -p "$OUT/pcsl3"
make -C "$PM/pcsl" PCSL_PLATFORM=javacall_i386_gcc PCSL_OUTPUT_DIR="$OUT/pcsl3" JDK_DIR="$JDK" > "$W/rec-pcsl.log" 2>&1
say "G2 CLDC-HI with the C interpreter (javacall_c; the commands are kept in rec-cldc.log)"
rm -rf "$OUT/cldc3"
( cd "$PM/cldc/build/javacall_c" && make A= JDK_DIR="$JDK" JVMWorkSpace="$PM/cldc" JVMBuildSpace="$OUT/cldc3" \
    ENABLE_MAP_FILE=false ENABLE_PCSL=true PCSL_OUTPUT_DIR="$OUT/pcsl3" > "$W/rec-cldc.log" 2>&1 )
say "G3 MIDP against it (rec-midp.log); the libjvm.a 'mergelib' step is not available: ignored"
J=$OUT/javacall3; rm -rf "$J"; mkdir -p "$J"
make -C "$PM/midp/build/javacall" A= JDK_DIR="$JDK" TOOLS_DIR="$PM/tools" CLDC_DIR="$PM/cldc" PCSL_DIR="$PM/pcsl" \
    JAVACALL_DIR="$PM/javacall" MIDP_OUTPUT_DIR="$J" PCSL_OUTPUT_DIR="$OUT/pcsl3" \
    CLDC_DIST_DIR="$OUT/cldc3/javacall_c/dist" USE_JSR_293=false JAVACALL_PLATFORM=linux_i386_gcc JAVACALL_OUTPUT_DIR="$JC" \
    USE_STATIC_PROPERTIES=true USE_DYNAMIC_PERMISSIONS=false -k > "$W/rec-midp.log" 2>&1 || true
test -x "$J/bin/i386/runMidlet"
say "G4 host check: the C-interpreter runMidlet runs HelloJ1"
rm -rf "$J/phehome" "$J/phecfg"; mkdir -p "$J/phehome" "$J/phecfg"; cp "$J/lib/skin.bin" "$J/appdb/_main.ks" "$J/phecfg/"
( cd "$J/bin/i386" && PHE_JAVA_HOME=$J/phehome PHE_JAVA_CONFIG=$J/phecfg timeout -s KILL 40 ./runMidlet -1 HelloJ1 \
    -classpathext "$W/j1/hello.jar" 2>&1 | grep -a '\[J1\]' )
say "G5 the Symbian .mmp"
rm -rf "$W/j3"
python3 "$HERE/gen_mmp_midp.py" "$W" "$PMS" "$W/j3"
cp "$W/j1/hello.jar" "$W/j3/data/hello.jar"
say "done: $W/j3 (group/phemidp.mmp, include/phemidp_x32.h, data/{skin.bin,_main.ks,hello.jar})"
