/*
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * phehost: javacall fonts from three built-in monospaced bitmap sets (font_data.c, ASCII; anything else draws as '?').
 * The stub implementation returned zero metrics, and LCDUI divides by them (the symbol table of the text input).
 */
#include "javacall_font.h"
#include "phe_font.h"

#ifdef __cplusplus
extern "C" {
#endif

static int cur_size = 1;        /* 0 small, 1 medium, 2 large */
static int cur_bold = 0;

static int pick(javacall_font_size size) {
    /* MIDP: SIZE_SMALL = 8, SIZE_MEDIUM = 0, SIZE_LARGE = 16 */
    return size == 8 ? 0 : size == 16 ? 2 : 1;
}

javacall_result javacall_font_set_font(javacall_font_face face, javacall_font_style style, javacall_font_size size) {
    (void)face;
    cur_size = pick(size);
    cur_bold = (style & JAVACALL_FONT_STYLE_BOLD) ? 1 : 0;
    return JAVACALL_OK;
}

javacall_result javacall_font_get_info(javacall_font_face face, javacall_font_style style, javacall_font_size size,
                                       /*out*/ int* ascent, /*out*/ int* descent, /*out*/ int* leading) {
    const phe_font_metrics* f = &phe_fonts[pick(size)];
    (void)face; (void)style;
    *ascent = f->ascent;
    *descent = f->descent;
    *leading = 1;
    return JAVACALL_OK;
}

int javacall_font_get_width(javacall_font_face face, javacall_font_style style, javacall_font_size size,
                            const javacall_utf16* charArray, int charArraySize) {
    const phe_font_metrics* f = &phe_fonts[pick(size)];
    (void)face; (void)charArray;
    return charArraySize * f->width + ((style & JAVACALL_FONT_STYLE_BOLD) ? 1 : 0);
}

javacall_result javacall_font_draw(javacall_pixel color, int clipX1, int clipY1, int clipX2, int clipY2,
                                   javacall_pixel* destBuffer, int destBufferHoriz, int destBufferVert,
                                   int x, int y, const javacall_utf16* text, int textLen) {
    const phe_font_metrics* f = &phe_fonts[cur_size];
    int i, row, col, pass;
    if (clipX1 < 0) clipX1 = 0;
    if (clipY1 < 0) clipY1 = 0;
    if (clipX2 > destBufferHoriz) clipX2 = destBufferHoriz;
    if (clipY2 > destBufferVert) clipY2 = destBufferVert;
    for (i = 0; i < textLen; i++) {
        unsigned c = text[i];
        const unsigned short* g;
        if (c < 32 || c > 126) c = '?';
        g = f->glyphs + (c - 32) * f->height;
        for (row = 0; row < f->height; row++) {
            int py = y + row;
            if (py < clipY1 || py >= clipY2) continue;
            for (col = 0; col < f->width; col++) {
                if (!(g[row] & (1 << (f->width - 1 - col)))) continue;
                for (pass = 0; pass <= cur_bold; pass++) {
                    int px = x + i * f->width + col + pass;
                    if (px >= clipX1 && px < clipX2) destBuffer[py * destBufferHoriz + px] = color;
                }
            }
        }
    }
    return JAVACALL_OK;
}

#ifdef __cplusplus
}
#endif
