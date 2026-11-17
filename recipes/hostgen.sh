#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: GPL-2.0-only
#
# hostgen.sh -- phoneME's own host build (cldc/build/linux_c: the portable C interpreter, 32-bit), which makes what the
# target build needs and cannot make itself:
#   - the class library (cldc_classes.zip) and the preverifier
#   - romgen, run on the host, and its output: the ROM image of the class library (ROMImage.cpp), the natives table,
#     jvmconfig.h and the merged VM sources (_MergedSrc*.cpp + incls/), all under $PM_OUT/linux_c/target/generated
# The host VM itself (dist/bin/cldc_vm) is the check that the tree works before the target build.
# Needs a JDK 8 (javac -source 1.3) and a 32-bit gcc (gcc-multilib): prepare.sh runs it in eclipse-temurin:8-jdk.
set -eu
. "$(dirname "$0")/env.sh"
[ -n "${JAVA_HOME:-}" ] || JAVA_HOME=$(dirname "$(dirname "$(readlink -f "$(command -v javac)")")")
cd "$PM_SRC/cldc/build/linux_c"
make host_arch=i386 JDK_DIR="$JAVA_HOME" JVMWorkSpace="$PM_SRC/cldc" JVMBuildSpace="$PM_OUT" ENABLE_MAP_FILE=false \
    > "$PM_ROOT/hostgen.log" 2>&1 || { tail -20 "$PM_ROOT/hostgen.log"; exit 1; }
# the host VM runs a preverified class: the tree builds and the ROM image boots
T=$PM_ROOT/hostcheck; rm -rf "$T"; mkdir -p "$T/c" "$T/v"
printf 'public class Hello { public static void main(String[] a) { System.out.println("[phoneme] host VM ok"); } }\n' > "$T/Hello.java"
LIB=$PM_OUT/linux_c/dist/lib/cldc_classes.zip
javac -nowarn -Xlint:-options -source 1.3 -target 1.1 -bootclasspath "$LIB" -d "$T/c" "$T/Hello.java"
"$PM_OUT/linux_c/dist/bin/preverify" -classpath "$LIB" -d "$T/v" "$T/c"
"$PM_OUT/linux_c/dist/bin/cldc_vm" -cp "$T/v" Hello
