#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: GPL-2.0-only
#
# fetch.sh -- the phoneME tree of sources.lock (the CLDC-HI VM and the class library: cldc/, legal/) into $PM_SRC at the
# pinned commit, with patches/ applied. Idempotent: a tree already at the commit is reset and re-patched.
set -eu
. "$(dirname "$0")/env.sh"
URL=$(lock_field 2); REV=$(lock_field 3); PATHS=$(lock_field 4)
if [ ! -d "$PM_SRC/.git" ]; then
    mkdir -p "$PM_ROOT"
    git clone -q --filter=blob:none --no-checkout "$URL" "$PM_SRC"
fi
cd "$PM_SRC"
git sparse-checkout set $PATHS
git -c advice.detachedHead=false checkout -q -f "$REV"
git reset -q --hard "$REV"
for p in "$PMS"/patches/*.patch; do git apply "$p"; done
echo "fetch.sh: phoneME $REV in $PM_SRC"
