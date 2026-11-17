# Copyright The Phorepoc Authors
# SPDX-License-Identifier: GPL-2.0-only

r"""components_phoneme -- the phoneme-symbian layer's components (merged into core's registry when this repository is built:
PHOREPOC_ADAPTATIONS=<board adaptations>:<phoneme-symbian>, see the pdk's tools/emitter/paths.py).

The VM's .mmp is generated (recipes/gen_mmp.py) from phoneME's own host build (recipes/hostgen.sh) under
$PM_ROOT (default $PHOREPOC_BUILD/phoneme): the phoneME tree itself is never copied into this repository.

The VM ships only as a SIS (recipes/sis.sh); no key here is a ROM component."""
import os

BUILD = os.environ.get("PHOREPOC_BUILD", "/build")
PM_ROOT = os.environ.get("PM_ROOT", os.path.join(BUILD, "phoneme"))
_HERE = os.path.dirname(os.path.abspath(__file__))
_PREFIX = os.path.join(_HERE, "..", "os", "src", "phoneme", "phoneme_x32.h")

# phoneME is 2007 C++: the switches phoneME's patched jvm.make gives a current gcc (patches/0001) plus its own -fno-rtti /
# -fno-exceptions (the VM uses neither)
_FLAGS = ["-include", _PREFIX, "-fpermissive", "-std=gnu++98", "-fno-strict-aliasing", "-fno-delete-null-pointer-checks",
          "-fno-lifetime-dse", "-fno-gnu-keywords", "-fno-operator-names", "-w"]

COMPONENTS = {
    # The phoneME Feature VM (CLDC-HI 1.1, the portable C interpreter) as a command-line launcher: cldc_vm.exe -cp <dir> <class>
    "phoneme_vm": dict(mmp=os.path.join(PM_ROOT, "group", "cldc_vm.mmp"), package_only=True, sis=True, allow_wsd=True,
                       static_ctors=True, rtti=False, flags=_FLAGS),
}
