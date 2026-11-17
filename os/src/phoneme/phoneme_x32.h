/*
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * phoneme_x32.h -- the build switches phoneME's linux_c makefiles pass on the compiler line (jvm.make: CPP_DEF_FLAGS for a
 * PRODUCT build of the C interpreter), forced into every VM source of the Phorepoc build (registry: -include). The VM runs
 * over Open C / P.I.P.S., so it takes phoneME's Linux OS layer (os/linux) as it is.
 */
#ifndef PHONEME_X32_H
#define PHONEME_X32_H
#define PRODUCT 1
#define GCC 1
#define LINUX 1
#define SUPPORTS_MONOTONIC_CLOCK 0
#define REQUIRES_JVMCONFIG_H 1
#define HARDWARE_LITTLE_ENDIAN 1
#define HOST_LITTLE_ENDIAN 1
#define ROMIZING 1
#define JVM_RELEASE_VERSION "1.1"
#define JVM_BUILD_VERSION "phorepoc"
#define JVM_NAME "phoneME Feature VM"
#endif
