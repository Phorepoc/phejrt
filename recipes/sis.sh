#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: GPL-2.0-only
#
# sis.sh -- after the components, in the toolchain container: the Java runtime and the demo suite as
#   $PHOREPOC_BUILD/sis/phejava-runtime-x32.sis   phemidp.exe, the Java configuration (skin), licences, NOTICE, source offer
#   $PHOREPOC_BUILD/sis/phejava-demo-x32.sis      the demo suite (two MIDlets): launchers, registrations, icon, jar; depends on the runtime
# with recipes/j5-pack.py (the pdk's tools do the resources and the icon: rcomp and mifconv are in the toolchain image).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); PMS=$(dirname "$HERE")
B=${PHOREPOC_BUILD:-/build}
J=${PHE_J_ROOT:-$B/jrt}
export PHOREPOC_PDK=${PHOREPOC_PDK:?PHOREPOC_PDK is not set}
V=${PHOREPOC_PACKAGE_VERSION:-0.3.0}
# Link stubs: a name the runtime calls that no provider exports is linked to a stub that panics the caller. The release fails on
# any name recipes/linkstubs.allow does not list ("name  # reason" lines).
for img in phemidp_sis.exe phejlaunch.exe; do
    STUBS=$B/x32img/$img.stubs
    if [ -s "$STUBS" ]; then
        BAD=$(grep -v '^$' "$STUBS" | while read -r n; do grep -qE "^$n([[:space:]]|$)" "$HERE/linkstubs.allow" 2>/dev/null || echo "$n"; done)
        [ -z "$BAD" ] || { echo "sis.sh: $STUBS has link stubs not in recipes/linkstubs.allow:"; echo "$BAD"; exit 1; }
    fi
done
mkdir -p "$B/sis"
python3 "$HERE/j5-pack.py" runtime --runtime "$B/x32img/phemidp_sis.exe" --cfg "$J/j3/data/cfg" \
    --runtime-version "$V" --out "$B/sis/phejava-runtime-x32.sis"
python3 "$HERE/j5-pack.py" suite --jad "$HERE/j1/hello.jad" --jar "$J/j3/data/hello.jar" --launcher "$B/x32img/phejlaunch.exe" \
    --runtime-version "$V" --version "$V" --out "$B/sis/phejava-demo-x32.sis"
ls -l "$B/sis"
