# Copyright The Phorepoc Authors
# SPDX-License-Identifier: GPL-2.0-only
"""j5-pack -- package a MIDlet suite (and optionally the runtime) as a SIS for Phorepoc: HOST side (Windows or Linux, Python 3).

    py recipes/j5-pack.py --suite-uid 0xE5A0A001 --name HelloJ1 --midlet HelloJ1 --jar hello.jar \\
        --launcher x32img/phejlaunch.exe --runtime x32img/phemidp_sis.exe --cfg <dir of skin.bin + PNGs> --out hello.sis

What a suite is on the phone (no hard-coded command line anywhere):
  * `!:\\sys\\bin\\phejl_<uid>.exe`     the template phejlaunch.exe retargeted to the suite's UID3 (and SID; no capabilities)
  * `!:\\private\\10003a3f\\import\\apps\\phejl_<uid>_reg.rsc` + `!:\\resource\\apps\\phejl_<uid>_loc.rsc`
                                       the AppArc registration: the menu lists it and starts the launcher
  * `!:\\private\\<uid>\\launch.txt`    the runtime's command line: `-1 <MIDlet class> -classpathext <jar>`
  * `!:\\private\\<uid>\\app.jar`       the suite
With --runtime the package also carries the runtime (phemidp.exe, the Java configuration, licences, NOTICE, source offer);
without it the suite package depends on the runtime package (--runtime-uid). Nothing is signed (Phorepoc installs unsigned).

Needs: the pdk's tools/emitter (sis.py, e32image.py, mmp.py; env PHOREPOC_PDK, default ../pdk), and for the registration
the private repo's rcomp (env PHE_REPO = the private repo, tools/rsc.py + tools/host/bin/rcomp.exe; run `py` there).
Source offer and licences: this repository's NOTICE, SOURCE-OFFER.txt, LICENSES/ (GPL-2.0-only; a source offer is part of
every package that carries the runtime).
"""
import argparse
import os
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
PMS = os.path.dirname(HERE)
PDK = os.environ.get("PHOREPOC_PDK", os.path.join(os.path.dirname(PMS), "pdk"))
sys.path.insert(0, os.path.join(PDK, "tools", "emitter"))
import e32image as E32   # noqa: E402
import mmp as MMP        # noqa: E402
import sis as SIS        # noqa: E402

RUNTIME_CAPS = ("NetworkServices", "ReadUserData", "WriteUserData", "UserEnvironment", "LocalServices")   # phemidp.mmp


def caps_bits(names):
    return sum(1 << MMP.Mmp.CAPS.index(n.lower()) for n in names)


def retarget(img, uid3, caps):
    """A copy of an x32 EXE with another UID3, the same SID, the given capability word, and a fresh UID checksum + header CRC."""
    b = bytearray(img)
    uid1, uid2 = struct.unpack_from("<II", b, 0)
    struct.pack_into("<I", b, 8, uid3)
    struct.pack_into("<I", b, 12, E32.uid_checksum(uid1, uid2, uid3))
    struct.pack_into("<I", b, 0x80, uid3)           # SSecurityInfo.iSecureId (E32ImageHeaderV)
    struct.pack_into("<II", b, 0x88, caps & 0xFFFFFFFF, 0)
    E32.set_header_crc(b)
    return bytes(b)


def with_caps(img, caps):
    b = bytearray(img)
    struct.pack_into("<II", b, 0x88, caps & 0xFFFFFFFF, 0)
    E32.set_header_crc(b)
    return bytes(b)


def compile_rss(text, name, out_rsc, uid3):
    repo = os.environ.get("PHE_REPO")
    if not repo:
        sys.exit("j5-pack: set PHE_REPO to the private repo checkout (rcomp: tools/rsc.py)")
    d = tempfile.mkdtemp()
    rss = os.path.join(d, name + ".rss")
    open(rss, "w").write(text)
    py = os.environ.get("PHE_PY", "py" if os.name == "nt" else "python3")
    r = subprocess.run([py, os.path.join(repo, "tools", "rsc.py"), rss, out_rsc], cwd=repo, capture_output=True, text=True)
    if r.returncode or not os.path.isfile(out_rsc):
        sys.exit("j5-pack: rcomp failed on %s:\n%s%s" % (name, r.stdout[-800:], r.stderr[-800:]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--suite-uid", type=lambda x: int(x, 0), required=True, help="the suite's application UID (also its package UID)")
    ap.add_argument("--name", required=True, help="the caption in the menu")
    ap.add_argument("--midlet", required=True, help="the MIDlet class to start")
    ap.add_argument("--jar", required=True)
    ap.add_argument("--launcher", required=True, help="phejlaunch.exe built with sis=True")
    ap.add_argument("--runtime", help="phemidp.exe built with sis=True: put the runtime into this package")
    ap.add_argument("--runtime-uid", type=lambda x: int(x, 0), default=0xE5A0F000, help="package UID of the runtime package (the dependency)")
    ap.add_argument("--cfg", help="directory with skin.bin, its PNGs and _main.ks (with --runtime)")
    ap.add_argument("--version", default="0.2.0")
    ap.add_argument("--drive", default="c", help="the install drive the launch.txt names (the installer's default is C:)")
    ap.add_argument("--runtime-only", action="store_true", help="only the runtime package (no suite)")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    uid = a.suite_uid
    ln = "phejl_%08x" % uid
    files = []
    lic = []
    for f in ["NOTICE", "SOURCE-OFFER.txt", "LICENSE"] + ["LICENSES/" + x for x in sorted(os.listdir(os.path.join(PMS, "LICENSES")))]:
        lic.append((open(os.path.join(PMS, f), "rb").read(), "!:\\resource\\phejava\\licences\\" + os.path.basename(f), None))
    pkg_uid, pkg_name = uid, a.name
    if not a.runtime_only:
        files.append((retarget(open(a.launcher, "rb").read(), uid, 0), "!:\\sys\\bin\\%s.exe" % ln, 0))
        reg = ('#include <appinfo.rh>\nUID2 KUidAppRegistrationResourceFile\nUID3 0x%08X\n'
               'RESOURCE APP_REGISTRATION_INFO\n    {\n    app_file = "%s";\n    localisable_resource_file = "\\\\resource\\\\apps\\\\%s_loc";\n    }\n') % (uid, ln, ln)
        loc = ('#include <appinfo.rh>\nRESOURCE LOCALISABLE_APP_INFO\n    {\n    short_caption = "%s";\n'
               '    caption_and_icon = CAPTION_AND_ICON_INFO\n        {\n        caption = "%s";\n        number_of_icons = 0;\n'
               '        icon_file = "";\n        };\n    }\n') % (a.name, a.name)
        td = tempfile.mkdtemp()
        compile_rss(reg, ln + "_reg", os.path.join(td, ln + "_reg.rsc"), uid)
        compile_rss(loc, ln + "_loc", os.path.join(td, ln + "_loc.rsc"), uid)
        files.append((open(os.path.join(td, ln + "_reg.rsc"), "rb").read(), "!:\\private\\10003a3f\\import\\apps\\%s_reg.rsc" % ln, None))
        files.append((open(os.path.join(td, ln + "_loc.rsc"), "rb").read(), "!:\\resource\\apps\\%s_loc.rsc" % ln, None))
        cmd = "-1 %s -classpathext %s:/resource/phejava/suites/%08x/app.jar\n" % (a.midlet, a.drive, uid)
        files.append((cmd.encode(), "!:\\private\\%08x\\launch.txt" % uid, None))
        # the jar must be readable by the runtime (another process): a private directory is not, \resource is
        files.append((open(a.jar, "rb").read(), "!:\\resource\\phejava\\suites\\%08x\\app.jar" % uid, None))
    deps = ()
    if a.runtime or a.runtime_only:
        pkg_uid = a.runtime_uid if a.runtime_only else uid
        if a.runtime_only:
            pkg_name = "phoneME MIDP runtime"
        files.append((with_caps(open(a.runtime, "rb").read(), caps_bits(RUNTIME_CAPS)), "!:\\sys\\bin\\phemidp.exe", caps_bits(RUNTIME_CAPS)))
        for f in sorted(os.listdir(a.cfg)):
            files.append((open(os.path.join(a.cfg, f), "rb").read(), "!:\\resource\\phejava\\cfg\\" + f, None))
        files += lic
    else:
        deps = [(a.runtime_uid, tuple(int(x) for x in a.version.split(".")), "phoneME MIDP runtime")]
        files += lic[:3]
    pkg = SIS.build_package(files, app_uid=pkg_uid, vendor="Phorepoc", app_name=pkg_name, signer=None,
                            version=tuple(int(x) for x in a.version.split(".")), dependencies=deps)
    open(a.out, "wb").write(pkg)
    print("%s: %d bytes, package uid %#x, %d files" % (a.out, len(pkg), pkg_uid, len(files)))
    for _, t, c in files:
        if "licences" not in t and "\\cfg\\" not in t:
            print("   ", t)


if __name__ == "__main__":
    main()
