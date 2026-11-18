# Copyright The Phorepoc Authors
# SPDX-License-Identifier: GPL-2.0-only
"""gen_mmp_midp -- the Symbian .mmp of the phoneME MIDP runtime (J3), from phoneME's own host build.

The host step (recipes/j3-gen.sh) builds PCSL, the C-interpreter CLDC-HI VM (cldc/build/javacall_c) and MIDP
(midp/build/javacall) for the javacall platform and keeps the compiler commands. This script turns them into one EXE:
every MIDP source the host build compiled, the VM sources of its libcldc_vm.a (minus the i386 assembler stubs), the
PCSL javacall modules and our javacall port (javacall/phehost). The macros go into one force-included header (the
quoting of -D'"x y"' does not survive an .mmp). The generated sources name each other by absolute path, so the x32
build must see the host step's tree at the same place ($J3_WORK, default /work).

    python3 gen_mmp_midp.py <work> <phejrt> <stage>

writes <stage>/group/phemidp.mmp, <stage>/include/phemidp_x32.h and <stage>/data/{skin.bin,_main.ks}.
"""
import glob
import os
import re
import shlex
import shutil
import sys

W, PMS, STAGE = sys.argv[1:4]
PM = W + "/pm"
OUT = W + "/out"
JAVACALL3 = OUT + "/javacall3"
CLDC_PROD = OUT + "/cldc3/javacall_c/target/product"
CLDC_GEN = OUT + "/cldc3/javacall_c/target/generated"
PCSL_INC = OUT + "/pcsl3/javacall_i386/inc"
JC_INC = OUT + "/jc2/inc"
PHEHOST = PMS + "/javacall/phehost"

defs = {}       # name -> value ("" = defined empty)
incs = []       # include directories, in first-seen order
cc_srcs = []    # (kind, path)


def add_inc(p):
    p = os.path.normpath(p)
    if p not in incs:
        incs.append(p)


def take_flags(tokens, base=None, cldc=False):
    for t in tokens:
        if t.startswith("-D"):
            n, _, v = t[2:].partition("=")
            if n in defs and not cldc:
                continue
            defs[n] = v
        elif t.startswith("-I"):
            p = t[2:]
            if p.startswith("."):
                p = os.path.join(base or "/", p)
            add_inc(p)


# ---- MIDP: the host build's compile commands -------------------------------------------------------------------
midp_srcs = [JAVACALL3 + "/generated/" + n for n in ("midp_constants_data.c", "lfj_image_rom.c", "properties_static_data.c")]
midp_srcs.append(JAVACALL3 + "/generated/classes/com/sun/midp/l10n/LocalizedStringsBase.c")   # generated, not in the compile log
for line in open(W + "/rec-midp.log", errors="replace"):
    if not re.match(r"g(cc|\+\+) ", line) or " -o" not in line:
        continue
    toks = shlex.split(line.replace("`", " "))
    take_flags(toks)
    m = re.search(r"-o(\S+\.o)\s+echo\s+(\S+)", line.replace("`", " "))
    if m:
        midp_srcs.append(m.group(2))
# ---- the VM: one product compile command for the flags, libcldc_vm.a's members for the sources -----------------
vm_line = [l for l in open(W + "/rec-cldc.log", errors="replace")
               if l.startswith("g++") and "-c /work/pm/cldc/src/vm/cpu/c/FloatSupport_c.cpp" in l][-1]   # the last = the product target
take_flags(shlex.split(vm_line), base=CLDC_PROD, cldc=True)   # the VM's own value wins (JVM_RELEASE_VERSION is quoted there)
VM = PM + "/cldc/src/vm"
VM_SRC = [VM + "/cpu/c/FloatSupport_c.cpp"]
for n in ("HotRoutines0", "HotRoutines1", "JarFileUncommon", "OsFile", "OsMemory"):
    VM_SRC.append(VM + "/share/runtime/%s.cpp" % n)
VM_SRC += [VM + "/share/natives/KniUncommon.cpp", VM + "/share/natives/PCSLSocket.cpp"]
for n in ("OS_javacall", "OsFile_javacall", "OsMemory_javacall", "OsMisc_javacall", "OsSocket_javacall"):
    VM_SRC.append(VM + "/os/javacall/%s.cpp" % n)
VM_SRC += sorted(glob.glob(CLDC_GEN + "/_MergedSrc*.cpp")) + [CLDC_PROD + "/OopMaps.cpp"]
add_inc(CLDC_GEN)
# ---- PCSL (javacall flavour) ------------------------------------------------------------------------------------
PCSL = PM + "/pcsl"
PCSL_SRC = [PCSL + "/print/javacall/pcsl_print.c", PCSL + "/memory/heap/pcsl_memory.c", PCSL + "/memory/heap/pcsl_chunkheap.c",
            PCSL + "/memory/memory_port/javacall/pcsl_memory_port.c", PCSL + "/file/javacall/pcsl_file.c",
            PCSL + "/file/javacall/pcsl_dir.c", PCSL + "/network/javacall/pcsl_network.c",
            PCSL + "/network/javacall/pcsl_socket.c", PCSL + "/network/javacall/pcsl_serversocket.c",
            PCSL + "/network/javacall/pcsl_datagram.c"]
PCSL_SRC += [PCSL + "/string/utf16/pcsl_string.c", PCSL + "/string/util/utf.c", PCSL + "/escfilenames/pcsl_esc.c"]
for d in ("print/javacall", "memory/heap", "memory/memory_port/javacall", "file/javacall", "network/javacall",
          "string", "string/utf16", "string/util", "escfilenames", "file", "network", "memory", "print"):
    add_inc(PCSL + "/" + d)
# ---- javacall: our port + the shared utilities ------------------------------------------------------------------
JC = PM + "/javacall"
JC_SRC = sorted(glob.glob(PHEHOST + "/common/*.c") + glob.glob(PHEHOST + "/common/*.cpp") + glob.glob(PHEHOST + "/midp/*.c"))
JC_SRC = [s for s in JC_SRC if os.path.basename(s) not in ("sos_network.c", "security.c")]   # duplicate definitions (the library picks one member)
JC_SRC += [JC + "/implementation/share/properties/javacall_properties.c",
           JC + "/implementation/share/properties/javacall_config_db.c",
           JC + "/implementation/share/properties/javacall_db.c", OUT + "/jc2/javacall_static_properties.c"]
JC_SRC += sorted(glob.glob(JC + "/implementation/share/utils/*.c"))
for d in (PHEHOST + "/common", PHEHOST + "/midp", JC + "/implementation/share/properties", JC + "/implementation/share/utils",
          JC + "/implementation/share/cldc", JC_INC):
    add_inc(d)
add_inc(PCSL_INC)
add_inc(JAVACALL3)
add_inc(JAVACALL3 + "/generated")

allsrc = []
for s in midp_srcs + VM_SRC + PCSL_SRC + JC_SRC:
    if os.path.isfile(s) and s not in allsrc:
        allsrc.append(s)
    elif not os.path.isfile(s):
        print("gen_mmp_midp: missing source " + s)

# ---- output -------------------------------------------------------------------------------------------------------
os.makedirs(STAGE + "/group", exist_ok=True)
os.makedirs(STAGE + "/include", exist_ok=True)
os.makedirs(STAGE + "/data", exist_ok=True)
hdr = ["/* generated by recipes/gen_mmp_midp.py: the macros of phoneME's javacall host build (MIDP + the C-interpreter VM) */",
       "#ifndef PHEMIDP_X32_H", "#define PHEMIDP_X32_H"]
SKIP = {"RELEASE", "FULL_VERSION"}
for n, v in sorted(defs.items()):
    if n in ("GCC", "LINUX", "UNIX", "ROMIZING") or n in SKIP:
        hdr.append("#ifndef %s\n#define %s %s\n#endif" % (n, n, v or "1"))
    else:
        hdr.append("#define %s %s" % (n, v))
hdr += ['#define RELEASE "phorepoc"', '#define FULL_VERSION "phorepoc"', "#endif"]
open(STAGE + "/include/phemidp_x32.h", "w", newline="\n").write("\n".join(hdr) + "\n")

def rel(p):
    return os.path.relpath(p, STAGE + "/group").replace(os.sep, "/")


bydir = {}
for s in allsrc:
    bydir.setdefault(os.path.dirname(s), []).append(os.path.basename(s))
mmp = ["// generated by recipes/gen_mmp_midp.py from phoneME's javacall host build -- do not edit",
       "TARGET          phemidp.exe", "TARGETTYPE      exe", "UID             0x100039CE 0xE5510A11",
       "CAPABILITY      NetworkServices ReadUserData WriteUserData UserEnvironment LocalServices", "VENDORID        0",
       "EPOCSTACKSIZE   0x20000", "EPOCHEAPSIZE    0x100000 0x6000000", ""]
for d, files in bydir.items():
    mmp.append("SOURCEPATH      " + rel(d))
    for i in range(0, len(files), 6):
        mmp.append("SOURCE          " + " ".join(files[i:i + 6]))
mmp.append("")
for d in incs:
    mmp.append("USERINCLUDE     " + rel(d))
mmp += ["", "OS_LAYER_SYSTEMINCLUDE_SYMBIAN", "OS_LAYER_LIBC_SYSTEMINCLUDE", "", "STATICLIBRARY   libcrt0.lib",
        "LIBRARY         euser.lib libc.lib libm.lib libpthread.lib libdl.lib", ""]
open(STAGE + "/group/phemidp.mmp", "w", newline="\n").write("#include <platform_paths.hrh>\n" + "\n".join(mmp))

shutil.copy(JAVACALL3 + "/lib/skin.bin", STAGE + "/data/skin.bin")
shutil.copy(JAVACALL3 + "/appdb/_main.ks", STAGE + "/data/_main.ks")
print("gen_mmp_midp: %d sources in %d directories, %d macros, %d include dirs -> %s/group/phemidp.mmp"
      % (len(allsrc), len(bydir), len(defs), len(incs), STAGE))
names = {}
for s in allsrc:
    names.setdefault(os.path.basename(s), []).append(s)
for n, l in names.items():
    if len(l) > 1:
        print("gen_mmp_midp: same file name %s: %s" % (n, l))
