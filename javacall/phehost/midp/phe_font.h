/*
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: GPL-2.0-only
 */
#ifndef PHE_FONT_H
#define PHE_FONT_H

#define PHE_NRANGES 4

typedef struct {
    int width;                          /* cell width in pixels (monospaced) */
    int height;                         /* cell height */
    int ascent;                         /* baseline row inside the cell */
    int descent;                        /* rows below the baseline */
    const unsigned short* glyphs;       /* the glyphs of phe_ranges, `height` rows each, bit (width-1-x) = pixel x */
} phe_font_metrics;

extern const phe_font_metrics phe_fonts[3];   /* small, medium, large */
extern const unsigned short phe_ranges[PHE_NRANGES][3];

#endif
