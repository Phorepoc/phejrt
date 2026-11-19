/*
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: GPL-2.0-only
 */
#ifndef PHE_FONT_H
#define PHE_FONT_H

typedef struct {
    int width;                          /* cell width in pixels (monospaced) */
    int height;                         /* cell height */
    int ascent;                         /* baseline row inside the cell */
    int descent;                        /* rows below the baseline */
    const unsigned short* glyphs;       /* 95 glyphs (ASCII 32..126) of `height` rows, bit (width-1-x) = pixel x */
} phe_font_metrics;

extern const phe_font_metrics phe_fonts[3];   /* small, medium, large */

#endif
