# Copyright The Phorepoc Authors
# SPDX-License-Identifier: GPL-2.0-only
"""j5-pack -- package a MIDlet suite (and the runtime) as SIS packages for Phorepoc: HOST side (Windows or Linux, Python 3).

    # the runtime on its own, then the suite (which depends on it); or --runtime with the suite = one combined package
    py recipes/j5-pack.py runtime --runtime phemidp_sis.exe --cfg <dir> --out phejava-runtime.sis
    py recipes/j5-pack.py suite --jad hello.jad --jar hello.jar --launcher phejlaunch.exe --out hello.sis
    py recipes/j5-pack.py suite --jad hello.jad --jar hello.jar --launcher phejlaunch.exe --runtime phemidp_sis.exe --cfg <dir> --out all.sis

The suite is described by its JAD (or, without --jad, by the jar's META-INF/MANIFEST.MF): MIDlet-1 ... MIDlet-n are
"name, icon, class" lines, and every MIDlet becomes an application of its own in the menu:
  * `!:\\sys\\bin\\phejl_<uid>.exe`   the template phejlaunch.exe with this application's UID3 and SID (no capabilities);
                                     the UIDs are the suite UID (JAD `Phorepoc-UID`, or --suite-uid) plus the MIDlet's index
  * `!:\\private\\10003a3f\\import\\apps\\phejl_<uid>_reg.rsc` + `!:\\resource\\apps\\phejl_<uid>_loc.rsc` + the suite's icon
    (`!:\\resource\\apps\\phejava_<suite uid>.mif`, from --icon or os/data/phejava_icon.svg, SVG-Tiny through mifconv)
  * `!:\\private\\<uid>\\launch.txt`  the runtime's command line: `-1 <class> -classpathext <jar>`
  * `!:\\resource\\phejava\\suites\\<suite uid>\\app.jar`   the suite (readable by the runtime: another process cannot read a
                                     private directory)
The runtime package carries phemidp.exe, the Java configuration (skin, keystore), the licences, NOTICE and the written
source offer; a suite package depends on it (--runtime-uid, --runtime-version). Nothing is signed (Phorepoc installs unsigned).

Needs: the pdk's tools/emitter (sis.py, e32image.py, mmp.py; env PHOREPOC_PDK, default ../pdk), and for the registration and
icon the private repo (env PHE_REPO: tools/rsc.py, tools/host/bin/{rcomp,mifconv,svgtbinencode}.exe; run on Windows or
with the Linux builds of those tools). Source offer and licences: this repository's NOTICE, SOURCE-OFFER.txt, LICENSES/.
"""
import argparse
import os
import struct
import subprocess
import sys
import tempfile
import zipfile

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


def repo():
    r = os.environ.get("PHE_REPO")
    if not r:
        sys.exit("j5-pack: set PHE_REPO to the private repo checkout (rcomp, mifconv: tools/host/bin)")
    return r


def compile_rss(text, name, out_rsc):
    d = tempfile.mkdtemp()
    rss = os.path.join(d, name + ".rss")
    open(rss, "w").write(text)
    py = os.environ.get("PHE_PY", "py" if os.name == "nt" else "python3")
    r = subprocess.run([py, os.path.join(repo(), "tools", "rsc.py"), rss, out_rsc], cwd=repo(), capture_output=True, text=True)
    if r.returncode or not os.path.isfile(out_rsc):
        sys.exit("j5-pack: rcomp failed on %s:\n%s%s" % (name, r.stdout[-800:], r.stderr[-800:]))


def make_mif(svg, out):
    """An SVG-Tiny file into an S60 .mif (NVG-encoded) with the release's mifconv + svgtbinencode."""
    bindir = os.path.join(repo(), "tools", "host", "bin")
    ext = ".exe" if os.name == "nt" else ""
    stage = tempfile.mkdtemp()
    stem = os.path.splitext(os.path.basename(out))[0]
    open(os.path.join(stage, stem + ".svg"), "wb").write(open(svg, "rb").read())
    sw = "/" if os.name == "nt" else "-"
    open(os.path.join(stage, "_params.txt"), "w", newline="\n").write("%sc32,8 %s.svg\n" % (sw, stem))
    mif = os.path.join(stage, stem + ".mif")
    r = subprocess.run([os.path.join(bindir, "mifconv" + ext), mif, "-E", "-I.", "-T.", "-S" + bindir, "-F" + os.path.join(stage, "_params.txt")],
                       cwd=stage, capture_output=True, text=True, errors="replace")
    if r.returncode or not os.path.isfile(mif):
        sys.exit("j5-pack: mifconv failed: " + (r.stdout + r.stderr)[-400:])
    return open(mif, "rb").read()


def parse_attrs(text):
    """JAD / manifest attributes (a manifest continues a line with a leading space)."""
    out, last = {}, None
    for line in text.replace("\r\n", "\n").split("\n"):
        if line.startswith(" ") and last:
            out[last] += line[1:]
        elif ":" in line:
            last, _, v = line.partition(":")
            out[last.strip()] = v.strip()
    return out


def midlets(attrs):
    """[(name, class)] from MIDlet-1 ... MIDlet-n ("name, icon, class")."""
    res, i = [], 1
    while ("MIDlet-%d" % i) in attrs:
        parts = [p.strip() for p in attrs["MIDlet-%d" % i].split(",")]
        res.append((parts[0], parts[-1]))
        i += 1
    return res


def licences():
    lic = []
    for f in ["NOTICE", "SOURCE-OFFER.txt", "LICENSE"] + ["LICENSES/" + x for x in sorted(os.listdir(os.path.join(PMS, "LICENSES")))]:
        lic.append((open(os.path.join(PMS, f), "rb").read(), "!:\\resource\\phejava\\licences\\" + os.path.basename(f), None))
    return lic


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("kind", choices=["suite", "runtime"])
    ap.add_argument("--jad")
    ap.add_argument("--jar")
    ap.add_argument("--suite-uid", type=lambda x: int(x, 0))
    ap.add_argument("--launcher", help="phejlaunch.exe built with sis=True")
    ap.add_argument("--icon", default=os.path.join(PMS, "os", "data", "phejava_icon.svg"))
    ap.add_argument("--runtime", help="phemidp.exe built with sis=True: carry the runtime in this package (always for `runtime`)")
    ap.add_argument("--runtime-uid", type=lambda x: int(x, 0), default=0xE5A0F000, help="package UID of the runtime package")
    ap.add_argument("--runtime-version", default="0.3.0")
    ap.add_argument("--cfg", help="directory with skin.bin, its PNGs and _main.ks (with the runtime)")
    ap.add_argument("--version", default=None, help="the suite's version (default: JAD MIDlet-Version)")
    ap.add_argument("--drive", default="c", help="the install drive launch.txt names (the installer's default is C:)")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    files, deps = [], ()
    ver_rt = tuple(int(x) for x in a.runtime_version.split("."))
    if a.kind == "runtime" or a.runtime:
        if not (a.runtime and a.cfg):
            sys.exit("j5-pack: the runtime needs --runtime and --cfg")
        files.append((with_caps(open(a.runtime, "rb").read(), caps_bits(RUNTIME_CAPS)), "!:\\sys\\bin\\phemidp.exe", caps_bits(RUNTIME_CAPS)))
        for f in sorted(os.listdir(a.cfg)):
            files.append((open(os.path.join(a.cfg, f), "rb").read(), "!:\\resource\\phejava\\cfg\\" + f, None))
        files += licences()
    if a.kind == "runtime":
        pkg = SIS.build_package(files, app_uid=a.runtime_uid, vendor="Phorepoc", app_name="phoneME MIDP runtime", signer=None, version=ver_rt)
        open(a.out, "wb").write(pkg)
        print("%s: %d bytes, runtime package uid %#x, %d files" % (a.out, len(pkg), a.runtime_uid, len(files)))
        return
    if not (a.jar and a.launcher):
        sys.exit("j5-pack: a suite needs --jar and --launcher")
    attrs = {}
    jar_bytes = open(a.jar, "rb").read()
    if a.jad:
        attrs = parse_attrs(open(a.jad, encoding="utf-8").read())
    else:
        with zipfile.ZipFile(a.jar) as z:
            attrs = parse_attrs(z.read("META-INF/MANIFEST.MF").decode("utf-8", "replace"))
    ms = midlets(attrs)
    if not ms:
        sys.exit("j5-pack: no MIDlet-1 in the JAD/manifest")
    base = a.suite_uid if a.suite_uid is not None else int(attrs.get("Phorepoc-UID", "0"), 0)
    if not base:
        sys.exit("j5-pack: no suite UID (--suite-uid or a Phorepoc-UID attribute)")
    suite_name = attrs.get("MIDlet-Name", ms[0][0])
    vendor = attrs.get("MIDlet-Vendor", "Phorepoc")
    version = tuple(int(x) for x in (a.version or attrs.get("MIDlet-Version", "1.0.0")).split("."))
    launcher = open(a.launcher, "rb").read()
    td = tempfile.mkdtemp()
    mif_name = "phejava_%08x.mif" % base
    files.append((make_mif(a.icon, os.path.join(td, mif_name)), "!:\\resource\\apps\\" + mif_name, None))
    suite_dir = "!:\\resource\\phejava\\suites\\%08x\\" % base
    files.append((jar_bytes, suite_dir + "app.jar", None))   # the runtime is another process: not in a private directory
    for i, (name, cls) in enumerate(ms):
        uid = base + i
        ln = "phejl_%08x" % uid
        files.append((retarget(launcher, uid, 0), "!:\\sys\\bin\\%s.exe" % ln, 0))
        reg = ('#include <appinfo.rh>\nUID2 KUidAppRegistrationResourceFile\nUID3 0x%08X\n'
               'RESOURCE APP_REGISTRATION_INFO\n    {\n    app_file = "%s";\n    localisable_resource_file = "\\\\resource\\\\apps\\\\%s_loc";\n    }\n') % (uid, ln, ln)
        loc = ('#include <appinfo.rh>\nRESOURCE LOCALISABLE_APP_INFO\n    {\n    short_caption = "%s";\n'
               '    caption_and_icon = CAPTION_AND_ICON_INFO\n        {\n        caption = "%s";\n        number_of_icons = 1;\n'
               '        icon_file = "\\\\resource\\\\apps\\\\%s";\n        };\n    }\n') % (name, name, mif_name)
        compile_rss(reg, ln + "_reg", os.path.join(td, ln + "_reg.rsc"))
        compile_rss(loc, ln + "_loc", os.path.join(td, ln + "_loc.rsc"))
        files.append((open(os.path.join(td, ln + "_reg.rsc"), "rb").read(), "!:\\private\\10003a3f\\import\\apps\\%s_reg.rsc" % ln, None))
        files.append((open(os.path.join(td, ln + "_loc.rsc"), "rb").read(), "!:\\resource\\apps\\%s_loc.rsc" % ln, None))
        cmd = "-1 %s -classpathext %s:/resource/phejava/suites/%08x/app.jar\n" % (cls, a.drive, base)
        files.append((cmd.encode(), "!:\\private\\%08x\\launch.txt" % uid, None))
    if not a.runtime:
        deps = [(a.runtime_uid, ver_rt, "phoneME MIDP runtime")]
        # the suite package must not install files of the runtime package (the clash fails the install: -30477, "file in use"):
        # its notices go to its own folder
        own = "!:\\resource\\phejava\\suites\\%08x\\licences\\" % base
        files += [(d, own + os.path.basename(t), c) for d, t, c in licences()[:3]]
    pkg = SIS.build_package(files, app_uid=base, vendor=vendor, app_name=suite_name, signer=None, version=version, dependencies=deps)
    open(a.out, "wb").write(pkg)
    print("%s: %d bytes, suite package uid %#x, %d MIDlets, %d files" % (a.out, len(pkg), base, len(ms), len(files)))
    for i, (name, cls) in enumerate(ms):
        print("    app %08x  %s  (%s)" % (base + i, name, cls))


if __name__ == "__main__":
    main()
