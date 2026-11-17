# Copyright The Phorepoc Authors
# SPDX-License-Identifier: GPL-2.0-only
#
# Sourced by the recipes.
#   PHOREPOC_BUILD   all generated files (default: ../core/build)
#   PM_SRC           the patched phoneME tree ($PHOREPOC_BUILD/phoneme/src); the host step's generated files name it by
#                    absolute path, so the target step must see it at the same path
#   PM_OUT           the host build (romgen, the generated ROM image and natives table, the class library)
PMS=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
: "${PHOREPOC_BUILD:=$PMS/../core/build}"
: "${PHOREPOC_PDK:=$PMS/../pdk}"
export PHOREPOC_BUILD PHOREPOC_PDK
: "${PM_ROOT:=$PHOREPOC_BUILD/phoneme}"
PM_SRC=$PM_ROOT/src
PM_OUT=$PM_ROOT/out
# lock_field N -- field N of the phoneME line of sources.lock
lock_field() { awk -F'\t' -v n="$1" '$1 == "phoneME" { print $n }' "$PMS/sources.lock"; }
