# phejrt

The Java runtime for Phorepoc OS: phoneME Feature (CLDC-HI 1.1 with its portable C interpreter, MIDP 2.1) and our own
"javacall" platform layer (window server, keys and touch, fonts, files, timers). A MIDlet suite installs as an ordinary
package; every MIDlet gets its own entry, name and icon in the menu. Two packages come out of this repository, through
Catalogue:

- `phejava-runtime-x32.sis` -- the runtime (phemidp.exe), its skin, the licences and the written source offer;
- `phejava-demo-x32.sis` -- two demo MIDlets (a form and a canvas) that depend on the runtime.

We don't keep a copy of phoneME here. `recipes/j1-host.sh` fetches the tree pinned in `sources.lock` and applies
`patches/`. This repository holds the patches, our javacall port (`javacall/phehost`), the build and packaging recipes and
the registry entries.

## Building (CI: `.github/workflows/release.yml`)

The pdk's `package.yml` builds `package.conf` on every push to main and releases what Conventional Commits ask for:

1. `recipes/prepare.sh` (on the runner, with a JDK 8 and gcc-multilib): `j1-host.sh` (phoneME's host build with our port),
   `j2-host.sh` (the headless run that checks it), `j3-gen.sh` (the C-interpreter VM, MIDP, and the Symbian `.mmp` from
   their compiler commands), all under `$PHOREPOC_BUILD/jrt`, bind-mounted at `/build` because the generated sources name
   each other by absolute path.
2. In the toolchain container: `phe component build phejlaunch phemidp_sis` (`registry/components_phejrt.py`).
3. `recipes/sis.sh` runs `recipes/j5-pack.py` for both packages (the toolchain image's rcomp and mifconv make the
   registration resources and the icon).

## What's in here

- `patches/`: phoneME's 2007 makefiles with a current gcc (0001), host-build fixes (0003), the C-interpreter configuration
  for the javacall platform (0004), a breakpoint that aborts on Symbian (0005), the x32 double word order (0006), a clean
  exit status (0007).
- `javacall/phehost`: our javacall implementation: libc memory and files, pthread events and timers, an RGB565 display with
  a window-server window on Symbian (phe_symbian.cpp), key and pointer input, built-in bitmap fonts.
- `os/src/phejlaunch`: the per-suite launcher (a copy per application, with its UID); `os/src/phejdrv`: the boot-test driver.
- `recipes/`: the build (`j1-host.sh`, `j2-host.sh`, `j3-gen.sh`, `gen_mmp_midp.py`), the packer (`j5-pack.py`), and the
  QEMU test scripts (`j4-volume.sh`, `j4-rom-boot.sh`).
- `catalogue/`: the store text and the unpublished listing drafts.

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

Result (J3b): on Phorepoc QEMU x86 the serial log shows `[J1] HelloJ1.startApp: MIDP MIDP-2.1, CLDC CLDC-1.1` and
`[J1] Form shown`. The ROM needs MEM=512; drive paths are Open C style (`z:/...`).


## J5: MIDlet suites as SIS packages (2026-11-19)

`recipes/j5-pack.py` (host, Python 3 + the pdk's `tools/emitter` + the private repo's rcomp) builds a SIS for a suite, and
optionally the runtime in the same package:

| File on the phone | What it is |
|---|---|
| `!:\sys\bin\phejl_<uid>.exe` | `phejlaunch.exe` (os/src/phejlaunch, 1.7 KB) copied with the suite's UID3 and SID, no capabilities |
| `!:\private\10003a3f\import\apps\phejl_<uid>_reg.rsc`, `!:\resource\apps\phejl_<uid>_loc.rsc` | the AppArc registration: the caption in the menu, the launcher as the application |
| `!:\private\<uid>\launch.txt` | the runtime's command line: `-1 <MIDlet> -classpathext c:/resource/phejava/suites/<uid>/app.jar` |
| `!:\resource\phejava\suites\<uid>\app.jar` | the suite (not in a private directory: the runtime is another process and could not read it) |
| `!:\sys\bin\phemidp.exe`, `!:\resource\phejava\cfg\*` | the runtime and its skin (with `--runtime`; else the package depends on the runtime package) |
| `!:\resource\phejava\licences\*` | NOTICE, LICENSE, GPL-2.0, phoneME and DejaVu/Bitstream notices, and `SOURCE-OFFER.txt` |

Starting an application from the menu makes AppArc run the launcher, which reads `launch.txt` and starts `phemidp.exe`; no
command line is built into the runtime. Tested on QEMU x86: `phejdrv.exe` (the boot-test driver) lets the real installer
(`sisinst`) install `c:\sisinst.sis`, waits until AppArc lists the UID, prints its caption and starts it with
`RApaLsSession::StartApp`, exactly the menu's call. RMS (`javax.microedition.rms`) stores its records under `c:\phejava`;
the test MIDlet prints the record count of a store it adds to. The built-in fonts now cover ASCII, Latin-1, Greek and
Cyrillic (343 glyphs per size).

## J6: JAD-aware packages, a runtime package and a suite package, uninstall (2026-11-19)

`recipes/j5-pack.py runtime|suite`:
- `suite --jad hello.jad --jar hello.jar --launcher phejlaunch.exe`: every `MIDlet-n: name, icon, class` of the JAD (or of the
  jar's MANIFEST.MF) becomes an application of its own, UID = suite UID (`Phorepoc-UID` or `--suite-uid`) + n-1: a launcher
  copy with that UID3, a registration and a localisable resource with the caption and the suite icon (an SVG-Tiny file,
  default `os/src/phejlaunch/phejava_icon.svg`, through mifconv into a `.mif`), and `launch.txt` with its class. The jar is installed once
  under `\resource\phejava\suites\<suite uid>\`.
- `runtime --runtime phemidp_sis.exe --cfg <dir>`: the runtime package (phemidp.exe, skin, licences, source offer). The suite
  package declares it as a dependency (`--runtime-uid`, `--runtime-version`); `suite --runtime ...` makes one combined package.
- A suite package must not carry files of the runtime package (same target = the install fails with -30477, "file in use"): its
  three notices live in `\resource\phejava\suites\<uid>\licences\`.
Test: `recipes/j4-rom-boot.sh` with `TESTEXE=phejdrv.exe DRVSCRIPT=<script> SISFILES="rt.sis=RT.SIS suite.sis=SUITE.SIS"`; the
driver (os/src/phejdrv) runs `install`, `install-fail`, `list`, `start`, `sleep`, `uninstall <package uid>`, `gone` lines.
The Catalogue listing drafts are in `catalogue/` (summary, description, `listing-draft/*.json`; not published).

## Status

Runs on Phorepoc x86 (QEMU): install, menu entry and icon, start from the menu, keys, touch, RMS on C:, uninstall.
Not done: networking, push, sound, the optional JSRs, OTA install from a JAD.

## Licence

GPL-2.0-only, the licence of phoneME (see `NOTICE`). phoneME's VM and class-library sources carry no Classpath
exception. The built-in fonts are rendered from DejaVu Sans Mono (Bitstream Vera licence, see `NOTICE`).
