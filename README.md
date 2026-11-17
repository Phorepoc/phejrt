# phoneme-symbian

The phoneME Feature VM (CLDC-HI 1.1, the portable C interpreter) for Phorepoc OS. It is the Java VM that Nokia's Java
runtime (the [jrt-symbian](https://github.com/Phorepoc/jrt-symbian) layer of the image) will run MIDlets on. The VM is
never part of an OS image: it ships as `phoneme-symbian-x32.sis` and installs from the Catalogue store, so an owner can
pick it or their own VM.

We don't keep a copy of phoneME here. `recipes/fetch.sh` fetches the tree pinned in `sources.lock` and applies
`patches/`. This repository holds the patches, the build recipes and the registry entry.

## Building

1. `recipes/prepare.sh` runs three steps:
   - `fetch.sh` fetches the pinned tree and applies the patches.
   - `hostgen.sh` runs phoneME's own 32-bit host build: the class library, romgen and the ROM image of the class
     library. As a check, it also runs a class on the host VM. It needs a JDK 8 and gcc-multilib, so outside such a
     container it runs in `eclipse-temurin:8-jdk`.
   - `gen_mmp.py` writes the VM's `.mmp` from that build.
2. In the toolchain container, with `PHOREPOC_ADAPTATIONS=...:<phoneme-symbian>`, build the component:
   `phe component build phoneme_vm` (or the pdk's `build_mmp_x32.py phoneme_vm`).
3. `recipes/sis.sh` makes `$PHOREPOC_BUILD/sis/phoneme-symbian-x32.sis`, with the licences, `NOTICE` and the written
   source offer.

## What's in here

- `patches/0001`: phoneME's 2007 makefiles with a current gcc and binutils.
- `patches/0002`: the Linux OS layer over Open C / P.I.P.S.:
  - no POSIX signals or ucontext (a timer thread drives the ticks);
  - heap chunks instead of anonymous mmap;
  - `fd_set` comes from `sys/select.h`;
  - a breakpoint or panic aborts the process.
- `os/src/phoneme/phoneme_x32.h`: the switches phoneME's makefiles put on the compiler line.

## Status

`cldc_vm.exe` links for x32 with 0 link stubs, and the SIS builds. It has not yet run on a device or in QEMU. The JNI
layer for the JRT natives and the `j9.dll` launcher come next (plan steps P3 to P5).

## Licence

GPL-2.0-only, the licence of phoneME (see `NOTICE`). phoneME's VM and class-library sources carry no Classpath
exception.
