#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: GPL-2.0-only
#
# j1-host.sh -- step J1: build phoneME MIDP on a Linux host (32-bit), two ways, and try to run a MIDlet.
#   stubs     midp/build/linux_stubs_gcc    (midp's own "stubs" platform; PCSL posix file + bsd/generic network)
#   javacall  midp/build/javacall + javacall/configuration/stubs/linux (all-stub javacall library, asm interpreter)
# Usage:  recipes/j1-host.sh docker   create container+volume, run the build inside, copy the log out, clean up
#         recipes/j1-host.sh build    run here (Ubuntu 24.04 with openjdk-8-jdk-headless gcc-multilib g++-multilib
#                                     make git gawk zip)
# Env: J1_WORK (default /work): the phoneME tree is $J1_WORK/pm, all output $J1_WORK/out.
set -eu
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd); PMS=$(dirname "$HERE")

if [ "${1:-}" = docker ]; then
    C=${J1_CONTAINER:-jrt-j1}; V=${J1_VOLUME:-jrt-j1-vol}; IMG=${J1_IMAGE:-ubuntu:24.04}
    docker volume create "$V" >/dev/null
    docker run -d --name "$C" --memory 6g -v "$V:/work" "$IMG" sleep infinity >/dev/null
    docker exec "$C" sh -c 'apt-get update -qq && apt-get install -y -qq openjdk-8-jdk-headless gcc-multilib g++-multilib make git gawk zip file ca-certificates >/dev/null'
    docker exec "$C" mkdir -p /work/phejrt
    ( cd "$PMS" && tar c --exclude=.git . ) | docker exec -i "$C" tar x -C /work/phejrt
    rc=0; docker exec "$C" bash /work/phejrt/recipes/j1-host.sh build || rc=$?
    docker cp "$C:/work/j1.log" ./j1.log 2>/dev/null || true
    docker rm -f "$C" >/dev/null; docker volume rm "$V" >/dev/null
    exit $rc
fi

W=${J1_WORK:-/work}; PM=$W/pm; OUT=$W/out; M32=$W/m32
JDK=${JDK_DIR:-/usr/lib/jvm/java-8-openjdk-amd64}
[ -x "$JDK/bin/javac" ] || { echo "j1-host.sh: no javac in $JDK (JDK_DIR must be a JDK 8)"; exit 1; }
export JAVA_HOME="$JDK" PATH="$JDK/bin:$PATH"
LOCK=$PMS/sources.lock
URL=$(awk -F'\t' '$1=="phoneME"{print $2}' "$LOCK"); REV=$(awk -F'\t' '$1=="phoneME"{print $3}' "$LOCK")
mkdir -p "$W"; LOG=$W/j1.log; : > "$LOG"
say() { echo "== $*" | tee -a "$LOG"; }

say "fetch phoneME $REV (the full tree: midp, pcsl, javacall and tools are needed, not only cldc)"
if [ ! -d "$PM/.git" ]; then git clone -q "$URL" "$PM"; fi
git -C "$PM" -c advice.detachedHead=false checkout -q -f "$REV"; git -C "$PM" reset -q --hard "$REV"
git -C "$PM" apply "$PMS/patches/0001-build-with-a-current-gcc.patch"     # cldc: i486 asm, current gcc
git -C "$PM" apply "$PMS/patches/0003-midp-host-build-fixes.patch"        # midp: four small fixes (see the patch)
git -C "$PM" apply "$PMS/patches/0004-cldc-c-interpreter-javacall-and-midp-assert.patch"   # J3: javacall_c (C interpreter) + ASSERT
patch -p1 -d "$PM" < "$PMS/patches/0005-cldc-breakpoint-symbian.patch" > /dev/null   # J3: a breakpoint aborts on Symbian (a hunk of 0002; applies with fuzz, hence patch(1))
patch -p1 -d "$PM" < "$PMS/patches/0006-cldc-double-word-order-not-arm.patch" > /dev/null   # J4: x32 doubles are little endian (Math.sqrt gave 0)
patch -p1 -d "$PM" < "$PMS/patches/0007-midp-javataskimpl-clean-exit.patch" > /dev/null   # J4: a MIDlet that ends itself is a clean exit

# 32-bit compiler wrappers: the makefiles call plain gcc/g++ and expect an i386 host. -static is dropped (the static libm
# of multilib glibc lacks fmod), warnings are off (2007 code), -lm and $EXTRA_LD are appended at link time.
mkdir -p "$M32"
for t in gcc g++; do
    if [ $t = gcc ]; then X='-fcommon -std=gnu89'; else X='-fpermissive -std=gnu++98'; fi
    cat > "$M32/$t" <<WRAP
#!/bin/bash
args=(); c=0
for a in "\$@"; do case "\$a" in -static) ;; -c) c=1; args+=("\$a");; *) args+=("\$a");; esac; done
if [ \$c = 1 ]; then exec /usr/bin/$t -m32 $X "\${args[@]}" -w -Wno-error; fi
exec /usr/bin/$t -m32 $X "\${args[@]}" -w -Wno-error \${EXTRA_LD:-} -lm
WRAP
    chmod +x "$M32/$t"
done
export PATH=$M32:$PATH GCC_LIBS="-lpthread -lm"

pcsl() {  # pcsl <outdir> <PCSL_PLATFORM> [make args]
    rm -rf "$1"; mkdir -p "$1"
    make -C "$PM/pcsl" PCSL_PLATFORM=$2 PCSL_OUTPUT_DIR="$1" JDK_DIR="$JDK" "${@:3}" >> "$LOG" 2>&1
}
midp() {  # midp <build dir> <out> <pcsl out> <cldc dist> [make args]; serial (a parallel first run races on generated files)
    make -C "$PM/midp/build/$1" JDK_DIR="$JDK" TOOLS_DIR="$PM/tools" CLDC_DIR="$PM/cldc" PCSL_DIR="$PM/pcsl" \
        JAVACALL_DIR="$PM/javacall" MIDP_OUTPUT_DIR="$2" PCSL_OUTPUT_DIR="$3" CLDC_DIST_DIR="$4" \
        USE_JSR_293=false "${@:5}" >> "$LOG" 2>&1
}
hello() {  # hello <midp out> <cldc classes.zip> <cldc dist>: compile, preverify and jar HelloJ1 against that build
    local CP=$1/classes.zip:$2
    mkdir -p "$W/j1/c" "$W/j1/v"
    javac -encoding UTF-8 -nowarn -source 1.3 -target 1.1 -bootclasspath "$CP" -d "$W/j1/c" "$HERE"/j1/*.java 2>&1 | grep -v warning || true
    "$3/bin/preverify" -classpath "$CP" -d "$W/j1/v" "$W/j1/c"
    ( cd "$W/j1/v" && jar cf ../hello.jar *.class )
}

# ---- A. stubs ----------------------------------------------------------------------------------------------------
S=$OUT/linux_stubs_gcc
say "A1 pcsl (linux_i386_gcc, posix file, bsd/generic network)"
pcsl "$S/pcsl" linux_i386_gcc NETWORK_MODULE=bsd/generic
say "A2 cldc linux_c (host_arch=i386, PCSL on)"
( cd "$PM/cldc/build/linux_c" && make host_arch=i386 JDK_DIR="$JDK" JVMWorkSpace="$PM/cldc" JVMBuildSpace="$OUT/cldc" \
    ENABLE_MAP_FILE=false ENABLE_PCSL=true PCSL_DIST_DIR="$S/pcsl/linux_i386" PCSL_OUTPUT_DIR="$S/pcsl" \
    ENABLE_ISOLATES=false >> "$LOG" 2>&1 )
CLDC=$OUT/cldc/linux_c/dist
say "A3 midp linux_stubs_gcc"
midp linux_stubs_gcc "$S" "$S/pcsl" "$CLDC"
test -x "$S/bin/i386/runMidlet"
hello "$S" "$CLDC/lib/cldc_classes.zip" "$CLDC"
say "A4 run HelloJ1 (stubs: a headless 240x320 display; it spins in the AMS loop, see the README)"
( cd "$S/bin/i386" && timeout -s KILL 20 ./runMidlet -1 HelloJ1 -classpathext "$W/j1/hello.jar" > "$W/j1/run-stubs.log" 2>&1 ) || true
grep -a '\[J1\]' "$W/j1/run-stubs.log" | tee -a "$LOG" || say "no [J1] line (the stub event layer never starts the MIDlet)"

# ---- B. javacall -------------------------------------------------------------------------------------------------
JC=$OUT/jc; J=$OUT/javacall
say "B1 javacall library (configuration/stubs/linux)"
mkdir -p "$JC"
( cd "$PM/javacall/configuration/stubs/linux" && make JAVACALL_DIR="$PM/javacall" TOOLS_DIR="$PM/tools" \
    JAVACALL_OUTPUT_DIR="$JC" USE_STATIC_PROPERTIES=true JDK_DIR="$JDK" >> "$LOG" 2>&1 )
export EXTRA_LD="-L$JC/lib -ljavacall -lpthread" JAVACALL_LIB_DIR=$JC/lib JAVACALL_OUTPUT_DIR=$JC JAVACALL_DIR=$PM/javacall
say "B2 pcsl javacall_i386_gcc + cldc javacall_i386_gcc"
pcsl "$J/pcsl" javacall_i386_gcc
( cd "$PM/cldc/build/javacall_i386_gcc" && make JDK_DIR="$JDK" JVMWorkSpace="$PM/cldc" JVMBuildSpace="$OUT/cldc_jc" \
    ENABLE_MAP_FILE=false ENABLE_PCSL=true PCSL_OUTPUT_DIR="$J/pcsl" >> "$LOG" 2>&1 )
say "B3 midp build/javacall (JAVACALL_PLATFORM=linux_i386_gcc; the final libjvm.a 'mergelib' step does not exist here: ignored)"
midp javacall "$J" "$J/pcsl" "$OUT/cldc_jc/javacall_i386_gcc/dist" JAVACALL_PLATFORM=linux_i386_gcc \
    JAVACALL_OUTPUT_DIR="$JC" USE_STATIC_PROPERTIES=true USE_DYNAMIC_PERMISSIONS=false -k || true
test -x "$J/bin/i386/runMidlet"
say "B4 run (the stub javacall main() is 'return 1': the VM is never started; expected)"
( cd "$J/bin/i386" && ./runMidlet -1 HelloJ1 -classpathext "$W/j1/hello.jar" ) > "$W/j1/run-javacall.log" 2>&1 || say "runMidlet exit $?"
say "done: stubs runMidlet = $S/bin/i386/runMidlet, javacall runMidlet = $J/bin/i386/runMidlet"
