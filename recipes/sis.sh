#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: GPL-2.0-only
#
# sis.sh -- after the components: the VM as $PHOREPOC_BUILD/sis/phoneme-symbian-x32.sis (cldc_vm.exe, the class library
# it was romized with is inside the executable; the licences, NOTICE and the written source offer beside it).
set -eu
. "$(dirname "$0")/env.sh"
KEY=phoneme_vm
# Link stubs: a name the VM calls that no provider exports is linked to a stub that panics the caller. The release fails
# on any name recipes/linkstubs.allow does not list ("name  # reason" lines).
STUBS=$PHOREPOC_BUILD/x32img/$KEY.exe.linkstubs
if [ -s "$STUBS" ]; then
    ALLOW=$(dirname "$0")/linkstubs.allow
    BAD=$(grep -v '^$' "$STUBS" | while read -r n; do grep -qE "^$n([[:space:]]|$)" "$ALLOW" 2>/dev/null || echo "$n"; done)
    [ -z "$BAD" ] || { echo "sis.sh: $STUBS has link stubs not in recipes/linkstubs.allow:"; echo "$BAD"; exit 1; }
fi
LIC=
for f in "$PMS"/LICENSES/* "$PMS/LICENSE" "$PMS/NOTICE" "$PMS/SOURCE-OFFER.txt"; do
    LIC="$LIC --file $f=!:/resource/phoneme/licences/$(basename "$f")"
done
mkdir -p "$PHOREPOC_BUILD/sis"
python3 "$PHOREPOC_PDK/tools/make_sis.py" $KEY --unsigned --version "${PHOREPOC_PACKAGE_VERSION:-0.1.0}" \
    --name "phoneME Java VM (CLDC-HI)" --vendor Phorepoc $LIC \
    --out "$PHOREPOC_BUILD/sis/phoneme-symbian-x32.sis"
