#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: GPL-2.0-only
#
# prepare.sh -- everything before the component build, on the CI runner (the pdk's package.yml runs it there, with docker and
# a JDK 8 at hand): phoneME's own host build with our javacall port (j1-host.sh), the headless run that checks it (j2-host.sh),
# and the Symbian .mmp generated from it (j3-gen.sh), all under $PHOREPOC_BUILD/jrt.
# The generated sources name each other by absolute path, so the tree must be at the path the toolchain container sees:
# /build/jrt. On the runner $PHOREPOC_BUILD is a directory; it is bind-mounted at /build.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
B=${PHOREPOC_BUILD:?PHOREPOC_BUILD is not set}
if [ ! -d "$B" ]; then echo "prepare.sh: PHOREPOC_BUILD must be a directory on the runner ($B)"; exit 1; fi
if command -v sudo >/dev/null 2>&1 && [ "$(id -u)" != 0 ]; then SUDO=sudo; else SUDO=; fi
if [ "$(readlink -f "$B")" != /build ]; then
    $SUDO mkdir -p /build
    mountpoint -q /build || $SUDO mount --bind "$B" /build
fi
$SUDO apt-get update -qq
$SUDO apt-get install -y -qq gcc-multilib g++-multilib gawk zip patch git >/dev/null
export JDK_DIR=${JAVA_HOME_8_X64:-/usr/lib/jvm/java-8-openjdk-amd64}
[ -x "$JDK_DIR/bin/javac" ] || { $SUDO apt-get install -y -qq openjdk-8-jdk-headless >/dev/null; JDK_DIR=/usr/lib/jvm/java-8-openjdk-amd64; export JDK_DIR; }
export J1_WORK=/build/jrt
mkdir -p "$J1_WORK"
bash "$HERE/j1-host.sh" build
J2_TIMEOUT=10 bash "$HERE/j2-host.sh"
bash "$HERE/j3-gen.sh"
ls -l "$J1_WORK/j3/group/phemidp.mmp" "$J1_WORK/j1/hello.jar"
