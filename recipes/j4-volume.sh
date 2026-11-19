#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: GPL-2.0-only
#
# j4-volume.sh <private-repo> [container] -- HOST side (Git Bash): make a volume-backed phorepoc-x86 container that holds
# exactly what recipes/j4-rom-boot.sh needs, by `tar | docker exec` (no bind mounts). Run it from any directory:
#   bash recipes/j4-volume.sh /c/Users/me/Git/Symbian-ReverseEngineered jrt8
# then copy the J-chain's /work tree in (docker exec <jchain> tar c ... | docker exec -i jrt8 tar x -C /), build phemidp
# (PHE_J3_WORK=/work MMP_JOBS=4 python3 emitter/build_mmp_x32.py phemidp) and run recipes/j4-rom-boot.sh in the container.
# About 1.5 GB: the private repo's tracked tree, the ~3600 files rom/full.args names, every .pcrel sidecar, .epoc32 and the
# baseline parts the kernel/ROM build and the x32 builder read.
set -eu
export MSYS_NO_PATHCONV=1
P=${1:?private repo checkout}; C=${2:-jrt-x32}
cd "$P"
B=.phorepoc-x86-build
docker image inspect phorepoc-x86 > /dev/null 2>&1 || docker build -q -t phorepoc-x86 - < tools/phorepoc-x86/Dockerfile
docker volume create "$C-vol" > /dev/null
docker run -d --name "$C" --memory 8g -v "$C-vol":/repo phorepoc-x86 sleep infinity > /dev/null
docker exec "$C" mkdir -p /repo/baseline/symbian-2010 /ext /work
# the files rom/full.args names that exist (awk: the line after --xip/--data/--builtin-device is name=path)
awk 'prev ~ /^--(xip|data|builtin-device)$/ && /=/ && !/^--/ { sub(/^[^=]*=/, ""); print } { prev = $0 }' $B/rom/full.args \
  | while IFS= read -r f; do [ -f "$f" ] && echo "$f"; done | grep -v '^C:' | grep -v '\.\./' | sort -u > /tmp/j4-refs.txt
{ git ls-files emitter toolchain tools shim src; cat /tmp/j4-refs.txt; echo $B/rom/full.args
  printf '%s\n' emitter/euser_x32.dll emitter/efsrv_x32.dll emitter/hal_x32.dll emitter/efile_x32.exe emitter/*.pcrel \
    $B/uexe/uhello.imp.exe $B/uexe/uhello.imp.exe.pcrel $B/x32img/e32strt.exe $B/x32img/phemidp.exe $B/j3/data/hello.jar; find $B/j3/data -type f
  ls $B/x32img/*.pcrel $B/x32img/*.meta $B/x32img/*.stubs; } | sort -u > /tmp/j4-all.txt
tar c -T /tmp/j4-all.txt 2> /dev/null | docker exec -i "$C" tar x -C /repo
tar c --exclude=.git baseline/kernelhwsrv baseline/symbian-2010/oss.FCL.sf.adapt.Qemu \
  baseline/symbian-2010/oss.FCL.sf.os.graphics/graphicsresourceservices baseline/symbian-2010/oss.FCL.sf.os.deviceplatformrelease \
  baseline/symbian-2010/oss.API_REF.Public_API baseline/symbian-2010/oss.FCL.sf.os.ossrv/genericopenlibs/openenvcore \
  baseline/symbian-2010/oss.FCL.sf.os.graphics/windowing/windowserver/group baseline/symbian-2010/oss.FCL.sf.os.graphics/windowing/windowserver/eabi baseline/symbian-2010/oss.FCL.sf.os.graphics/fbs/fontandbitmapserver/group baseline/symbian-2010/oss.FCL.sf.os.graphics/fbs/fontandbitmapserver/eabi baseline/symbian-rcl3/oss.FCL.sf.mw.appsupport/appfw/apparchitecture/group baseline/symbian-rcl3/oss.FCL.sf.mw.appsupport/appfw/apparchitecture/eabi \n  .epoc32 $B/mmp-x32/libcrt0 third_party/symbian2010-camerasc | docker exec -i "$C" tar x -C /repo
# full.args also names files below baseline/symbian-2010/../ (= baseline/) and a few outside the repo (/ext)
awk 'prev ~ /^--(xip|data)$/ && /=/ { sub(/^[^=]*=/, ""); if ($0 ~ /\.\.\//) print } { prev = $0 }' $B/rom/full.args \
  | sed 's#baseline/symbian-2010/\.\./#baseline/#' | while IFS= read -r f; do [ -f "$f" ] && echo "$f"; done | tar c -T - 2> /dev/null | docker exec -i "$C" tar x -C /repo || true
echo "volume ready: $C ($C-vol)"
# the files full.args names outside the repo: <home>\Phorepoc\<repo>/os/<path> (the public repos' checkouts, next to the private
# one) and two of the private repo's own, remapped to /ext (j4-rom-boot.sh does the same remapping of the names)
PUB=${PHOREPOC_REPOS:-$P/../Phorepoc}
awk 'prev ~ /^--(xip|data)$/ && /=C:/ { sub(/^[^=]*=/, ""); print } { prev = $0 }' $B/rom/full.args | tr -d '\r' | while IFS= read -r f; do
  case "$f" in
    *Phorepoc*catalogue/os/*)    rel=${f#*catalogue/os/};    src="$PUB/catalogue/os/$rel" ;;
    *Phorepoc*dbus-symbian/os/*) rel=${f#*dbus-symbian/os/}; src="$PUB/dbus-symbian/os/$rel" ;;
    *device.txt)                 rel=device.txt;             src="$P/$B/rom/device.txt" ;;
    *Choices.x32)                rel=Choices.x32;            src="$P/tools/phorepoc-x86/netsurf/engine/Choices.x32" ;;
    *) continue ;;
  esac
  if [ -f "$src" ]; then docker exec "$C" mkdir -p "/ext/$(dirname "$rel")"; docker cp "$(cygpath -m "$src" 2>/dev/null || echo "$src")" "$C:/ext/$rel"; fi
done
echo "/ext done"
# a Windows checkout keeps ProductVariant.hrh in lower case; the mmp preprocessor of the x32 builder is case sensitive on Linux
docker exec "$C" sh -c 'cd /repo/.epoc32/include && [ -f ProductVariant.hrh ] || cp productvariant.hrh ProductVariant.hrh'
