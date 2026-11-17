#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: GPL-2.0-only
#
# prepare.sh -- everything before the component build: fetch.sh (the pinned, patched phoneME tree), hostgen.sh (phoneME's
# own host build: class library, romgen and the ROM image) and gen_mmp.py (the VM's .mmp).
# hostgen.sh needs a JDK 8 and gcc-multilib, which the toolchain image does not carry: inside a container that has them
# (HOSTGEN_HERE=1) it runs directly, otherwise in eclipse-temurin:8-jdk with the build tree as a named volume.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/env.sh"
bash "$HERE/fetch.sh"
if [ "${HOSTGEN_HERE:-}" = 1 ] || { command -v javac >/dev/null && gcc -m32 -E -x c /dev/null >/dev/null 2>&1; }; then
    bash "$HERE/hostgen.sh"
else
    : "${PM_VOLUME:?set PM_VOLUME to the docker volume that holds PM_ROOT (mounted at the same path)}"
    docker run --rm -v "$PM_VOLUME:$(dirname "$PM_ROOT")" -v "$PMS:/pms:ro" -e PM_ROOT="$PM_ROOT" \
        -e PHOREPOC_BUILD="$PHOREPOC_BUILD" eclipse-temurin:8-jdk sh -c \
        'apt-get update -qq && apt-get install -y -qq make g++-multilib >/dev/null && cp -r /pms /tmp/pms && bash /tmp/pms/recipes/hostgen.sh'
fi
python3 "$HERE/gen_mmp.py" "$PM_ROOT" "$PMS"
