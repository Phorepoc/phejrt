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

## J1: phoneME MIDP on a Linux host (2026-11-18)

`recipes/j1-host.sh docker` (or `build` inside an Ubuntu 24.04 container with `openjdk-8-jdk-headless gcc-multilib
g++-multilib make git gawk zip`) builds, from the full phoneME tree at the pinned commit, with `patches/0001` and
`patches/0003` applied:

| Step | Result |
|---|---|
| PCSL (`linux_i386_gcc`, posix file, `bsd/generic` network) | builds |
| CLDC-HI (`linux_c`, i386, PCSL on) | builds: romgen, preverifier, `libcldc_vm.a`, `cldc_classes.zip` |
| MIDP `midp/build/linux_stubs_gcc` | builds and links `runMidlet` (full MIDP class library, romized) |
| javacall library `javacall/configuration/stubs/linux` | builds (`libjavacall.a`, all stubs) |
| MIDP `midp/build/javacall` (`JAVACALL_PLATFORM=linux_i386_gcc`) | builds and links `runMidlet` against CLDC `javacall_i386_gcc` (asm interpreter) |

What runs: the stubs `runMidlet` starts the VM and the MIDP system, loads and runs the romized MIDP classes (LCDUI skin
loading logs "no resource" for the images: the stubs platform has none). With the headless display of `0003` (one
240x320 display) it gets as far as the AMS loop and then spins: the stubs' event queue delivers no foreground/start
events, so `HelloJ1.startApp` is not reached. The javacall `runMidlet` exits at once: the stub `main()` in
`javacall/implementation/stubs/midp/main.c` is `return 1`.

**Finding: the mirror has no functional `linux_x86` javacall port.** `javacall/implementation/linux_x86` holds only
`javacall_platform_defs.h`; `linux_x86_emul` only a JSR 177 card simulator, `linux_x86_cdc` only JSR 120/135/CDC
pieces. The one functional MIDP javacall port is `win32` (GDI + Win32 events). `midp/build/linux_fb_gcc` is a real Linux
port but draws to a Qt virtual framebuffer (it needs `/tmp/.qtvfb_mouse-0` and qvfb). So a MIDlet on the host needs a
small headless javacall port of our own (os/time, event queue, lifecycle start, a memory lcd): that is the first
deliverable of J2/J3, and the same code is what the Symbian port starts from.

Recipe notes (the makefiles are 2007): they need explicit `TOOLS_DIR`, `CLDC_DIR`, `PCSL_DIR`, `JAVACALL_DIR`,
`CLDC_DIST_DIR`, `PCSL_OUTPUT_DIR` and `USE_JSR_293=false` (missing from the options file); PCSL and CLDC are built first;
a 32-bit gcc wrapper replaces `-static` (the static multilib libm lacks `fmod`); the first MIDP run must be serial
(parallel `-j` races on generated headers). `patches/0003`: `Key` -> `KeymapKey` (stub keymap), `--end-group` without
`-Xlinker`, `socket_notify` also for the `stubs` platform, and a headless display in the stub `lfjport`.

## J2: our headless javacall port, HelloJ1 on the host (2026-11-18)

`javacall/phehost` is our own javacall implementation for a Linux host (GPL-2.0-only; the structure follows the
phoneME `stubs` directory, the code is new): libc memory, POSIX file/dir (UTF-16 names to UTF-8; `$PHE_JAVA_HOME`,
`$PHE_JAVA_CONFIG`), pthread os/mutex/condition, a blocking event queue (`javacall_event_send/receive`), the event-queue
lock, timers (one thread each), lifecycle logging, logging to stdout, and one headless RGB565 display (240x320, or
`$PHE_LCD_SIZE`; `$PHE_LCD_DUMP` = a PPM of every flush). `main()` runs `JavaTask` on a thread and posts the start event
with the command line (midp's `runMidlet` argument layout).

`recipes/j2-host.sh` (run after `j1-host.sh build`) builds it as the javacall library, relinks `runMidlet` and runs
HelloJ1 (a Form, then `notifyDestroyed`):

    [J1] HelloJ1.startApp: MIDP MIDP-2.1, CLDC CLDC-1.1
    [J1] Form shown

The form is drawn by phoneME's Java look-and-feel into the frame buffer (the PPM has 366 distinct colours). Known gap:
`JavaTaskImpl` reports `[phehost] lifecycle state 13 status -1` at the end (shutdown with a failure code: the
`runMidlet` return value is not the `1` that `JavaTaskImpl` expects; the MIDlet itself ended normally). Not done yet:
key/pen events into the queue, network, push, fonts other than the skin's, sound.

x32 (`recipes/j2-x32-compile.sh`): PCSL, the phehost library (42 objects) and all 155 MIDP C objects compile with
`-mx32` and no errors (ELF32, x86-64); no link, because the CLDC (asm interpreter, `javacall_i386_gcc`) is i386 and an
x32 binary cannot run in Docker Desktop's kernel. The Symbian-side build (the `.mmp` from this build, as P1 did for
CLDC, with the C interpreter) is the next step.

## J3: the MIDP runtime as one Symbian x32 EXE (2026-11-18, in progress)

`recipes/j3-gen.sh` (after `j1-host.sh build` and `j2-host.sh`) builds PCSL, the **C-interpreter** CLDC-HI VM (new
`cldc/build/javacall_c`, `patches/0004`) and MIDP for the javacall platform with the phehost port, checks that the host
`runMidlet` still runs HelloJ1, keeps the compiler commands, and `gen_mmp_midp.py` writes `phemidp.mmp`: 223 sources
(152 MIDP, the VM's own, PCSL's javacall modules, javacall/phehost) with the macros in one force-included header.
The phehost port builds unchanged for Symbian (Open C: pthreads, POSIX files); `#ifdef __SYMBIAN32__` selects the debug
port for output (`phe_symbian.cpp`: RDebug::RawPrint), Z:/C: default directories (`/z/phejava/cfg`, `/c/phejava`),
no `statvfs`, and the built-in command line `runMidlet -1 HelloJ1 -classpathext /z/phejava/hello.jar`.

The x32 build (Phorepoc's `emitter/build_mmp_x32.py`, key `phemidp`, `emitter/components_phemidp.py` in the private
repo) gives `phemidp.exe`: 1.49 MB, imports euser, libc, libm and libpthread only, **0 link stubs**. Two more patches
were needed: `0004` also makes e32's empty `ASSERT` not break `midpInflateint.h`; `0005` (a hunk of 0002) makes a VM
breakpoint abort instead of `int $3`. MMP_JOBS=4: the merged VM sources are big.

Not done: the ROM (`SSM_TEST=phemidp`) and a QEMU run. HelloJ1's lines will appear on the serial port as `[J1] ...`.

## Status

`cldc_vm.exe` links for x32 with 0 link stubs, and the SIS builds. It has not yet run on a device or in QEMU. The JNI
layer for the JRT natives and the `j9.dll` launcher come next (plan steps P3 to P5).

## Licence

GPL-2.0-only, the licence of phoneME (see `NOTICE`). phoneME's VM and class-library sources carry no Classpath
exception.
