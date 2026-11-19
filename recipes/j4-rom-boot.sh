#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: GPL-2.0-only
#
# j4-rom-boot.sh -- one shot, INSIDE the x32 container: the Phorepoc ROM with phemidp as the SSM test EXE, then QEMU.
#
# It reuses the last full Phorepoc ROM's argument file (rom/full.args of the private repo's build dir) and adds
# phemidp.exe as Sys\Bin\ssmtest.exe (SSM's non-critical list starts it) + its data under Z:\phejava, runs tools/rombuild.py
# for the section, builds the kernel + ISO and boots one QEMU (tools/phorepoc-x86/ekern/make_ekern.sh run).
# Needs, in $REPO (see recipes/j4-volume.sh for the copy list): the private repo's tracked tree, the files full.args names
# (+ every .pcrel sidecar), .phorepoc-x86-build/x32img/phemidp.exe (+ .pcrel), j3/data/{skin.bin,_main.ks,hello.jar},
# /ext (the few files full.args names outside the repo). MEM must be 512: the section alone is 325 MB.
#
# Env: REPO (/repo)  SECS (420, QEMU run time)  SHOT_ON ("[J1]": start the scripted steps when the log shows it)
#      MONCMDS (qmon steps, e.g. 'wait:[J1] Form shown;sleep:3;shot:a;sendkey 5;sleep:2;shot:b')  SHOT_QUIT (1)
set -eu
REPO=${REPO:-/repo}; cd "$REPO"
B=.phorepoc-x86-build
python3 - <<'PY'
import os
BS = chr(92)
B = ".phorepoc-x86-build"
s = open(B + "/rom/full.args").read()
pre = "C:" + BS + "Users" + BS + "memph" + BS + "Git" + BS
for a, b in ((pre + "Phorepoc" + BS + "catalogue/os/", "/ext/"), (pre + "Phorepoc" + BS + "dbus-symbian/os/", "/ext/"),
             (pre + "Symbian-ReverseEngineered" + BS + B + BS + "rom" + BS + "device.txt", "/ext/device.txt"),
             (pre + "Symbian-ReverseEngineered" + BS + "tools/phorepoc-x86/netsurf/engine/Choices.x32", "/ext/Choices.x32")):
    s = s.replace(a, b)
s = s.rstrip("\n") + "\n" + "\n".join([
    "--xip", "Sys" + BS + "Bin" + BS + "ssmtest.exe=" + B + "/x32img/phemidp.exe",
    *[x for f in sorted(os.listdir(B + "/j3/data/cfg")) for x in ("--data", "phejava" + BS + "cfg" + BS + f + "=" + B + "/j3/data/cfg/" + f)],
    "--data", "phejava" + BS + "hello.jar=" + B + "/j3/data/hello.jar", ""])
L, out, i = s.split("\n"), [], 0
while i < len(L):                    # a file that is not here either (the original ROM listed one that was missing): drop its pair
    if i + 1 < len(L) and L[i] in ("--data", "--xip") and ("C:" + BS) in L[i + 1]:
        print("dropped:", L[i + 1][:100]); i += 2; continue
    out.append(L[i]); i += 1
open(B + "/rom/full2.args", "w").write("\n".join(out))
PY
python3 tools/rombuild.py --section 0x80400000 @$B/rom/full2.args \
  --xip 'Sys\Bin\euser.dll=emitter/euser_x32.dll' --xip 'Sys\Bin\efsrv.dll=emitter/efsrv_x32.dll' \
  --xip 'Sys\Bin\hal.dll=emitter/hal_x32.dll' --xip 'Sys\Bin\efile.exe=emitter/efile_x32.exe' \
  --xip 'Sys\Bin\estart.exe='$B/x32img/e32strt.exe --xip 'Sys\Bin\uhello.exe='$B/uexe/uhello.imp.exe \
  --builtin-device 'Sys\Bin\enet.ldd=ldd:1' --builtin-device 'Sys\Bin\ethernet.pdd=pdd:2' \
  --builtin-device 'Sys\Bin\esoundsc.ldd=ldd:3' --builtin-device 'Sys\Bin\soundsc.pdd=pdd:4' \
  --builtin-device 'Sys\Bin\ecamerasc.ldd=ldd:5' --builtin-device 'Sys\Bin\camerasc.pdd=pdd:6' \
  --builtin-device 'Sys\Bin\sgdevice.ldd=ldd:7' --builtin-device 'Sys\Bin\pipelib.ldd=ldd:8' \
  --secondary-name efile.exe $B/rom/romsect.bin > /tmp/rombuild.log 2>&1 || { tail -5 /tmp/rombuild.log; exit 1; }
tail -1 /tmp/rombuild.log
# the kernel + ISO + one QEMU, as `./ph boot` does (with a persistent-less C: made from the blank template)
ln -sfn "$REPO/$B" /out
mkdir -p /out/disk /out/ekern
[ -f /out/disk/regress-template.img ] || IMG=/out/disk/regress-template.img bash tools/phorepoc-x86/make_disk.sh > /dev/null
cp /out/disk/regress-template.img /out/disk/boot.img
export SECONDARY=rom SCREEN=${SCREEN:-360x640} SCALE=${SCALE:-2} MEM=512 DISK=1 DISKIMG=/out/disk/boot.img SHOT=1 \
       SHOT_WAIT=${SHOT_WAIT:-380} SECS=${SECS:-420} LOGMAX=4000000 KCLEAN=0 UARTBATCH=0 DUMP=0 SLIDE=0 \
       SHOT_ON="${SHOT_ON:-[J1]}" SHOT_QUIT=${SHOT_QUIT:-1} MONCMDS="${MONCMDS:-}"
bash tools/phorepoc-x86/ekern/make_ekern.sh run > /tmp/boot.log 2>&1 || true
tail -3 /tmp/boot.log | cut -c1-200
grep -a '\[J1\]\|\[phehost\]' /out/ekern/run.log | tr -cd '\11\12\15\40-\176' | head -40
