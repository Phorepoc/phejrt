/*
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * phehost: one headless display, an RGB565 frame buffer in memory (default 240x320, $PHE_LCD_SIZE=WxH).
 * javacall_lcd_flush writes the frame to the PPM file $PHE_LCD_DUMP (when set), so a host run can be inspected.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "javacall_lcd.h"

#ifdef __cplusplus
extern "C" {
#endif

static javacall_pixel* fb = NULL;
static int fb_w = 240, fb_h = 320;
static int screen_ids[] = { 0 };

javacall_result javacall_lcd_init(void) {
    const char* s;
    if (fb) return JAVACALL_OK;
    s = getenv("PHE_LCD_SIZE");
    if (s) { int w, h; if (sscanf(s, "%dx%d", &w, &h) == 2 && w > 0 && h > 0) { fb_w = w; fb_h = h; } }
    fb = (javacall_pixel*)calloc((size_t)fb_w * (size_t)fb_h, sizeof(javacall_pixel));
    return fb ? JAVACALL_OK : JAVACALL_OUT_OF_MEMORY;
}

javacall_result javacall_lcd_finalize(void) {
    free(fb); fb = NULL;
    return JAVACALL_OK;
}

javacall_pixel* javacall_lcd_get_screen(int hardwareId, int* screenWidth, int* screenHeight,
                                        javacall_lcd_color_encoding_type* colorEncoding) {
    (void)hardwareId;
    if (!fb && javacall_lcd_init() != JAVACALL_OK) return NULL;
    *screenWidth = fb_w; *screenHeight = fb_h;
    *colorEncoding = JAVACALL_LCD_COLOR_RGB565;
    return fb;
}

static void dump(int y0, int y1) {
    const char* path = getenv("PHE_LCD_DUMP");
    FILE* f;
    int x, y;
    (void)y0; (void)y1;
    if (!path || !fb) return;
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", fb_w, fb_h);
    for (y = 0; y < fb_h; y++) {
        for (x = 0; x < fb_w; x++) {
            unsigned p = fb[y * fb_w + x];
            unsigned char rgb[3];
            rgb[0] = (unsigned char)(((p >> 11) & 31) * 255 / 31);
            rgb[1] = (unsigned char)(((p >> 5) & 63) * 255 / 63);
            rgb[2] = (unsigned char)((p & 31) * 255 / 31);
            fwrite(rgb, 1, 3, f);
        }
    }
    fclose(f);
}

javacall_result javacall_lcd_flush(int hardwareId) { (void)hardwareId; dump(0, fb_h); return JAVACALL_OK; }

javacall_result javacall_lcd_flush_partial(int hardwareId, int ystart, int yend) {
    (void)hardwareId; dump(ystart, yend); return JAVACALL_OK;
}

javacall_result javacall_lcd_set_full_screen_mode(int hardwareId, javacall_bool useFullScreen) {
    (void)hardwareId; (void)useFullScreen; return JAVACALL_OK;
}

javacall_bool javacall_lcd_reverse_orientation(int hardwareId) { (void)hardwareId; return JAVACALL_FALSE; }
void javacall_lcd_handle_clamshell() {}
javacall_bool javacall_lcd_get_reverse_orientation(int hardwareId) { (void)hardwareId; return JAVACALL_FALSE; }
javacall_bool javacall_lcd_is_native_softbutton_layer_supported() { return JAVACALL_FALSE; }

javacall_result javacall_lcd_set_native_softbutton_label(const javacall_utf16* label, int len, int index) {
    (void)label; (void)len; (void)index; return JAVACALL_FAIL;
}

int javacall_lcd_get_screen_width(int hardwareId) { (void)hardwareId; return fb_w; }
int javacall_lcd_get_screen_height(int hardwareId) { (void)hardwareId; return fb_h; }
int javacall_lcd_get_current_hardwareId() { return 0; }
char* javacall_lcd_get_display_name(int hardwareId) { (void)hardwareId; return (char*)"phehost"; }
javacall_bool javacall_lcd_is_display_primary(int hardwareId) { (void)hardwareId; return JAVACALL_TRUE; }
javacall_bool javacall_lcd_is_display_buildin(int hardwareId) { (void)hardwareId; return JAVACALL_TRUE; }
javacall_bool javacall_lcd_is_display_pen_supported(int hardwareId) { (void)hardwareId; return JAVACALL_TRUE; }
javacall_bool javacall_lcd_is_display_pen_motion_supported(int hardwareId) { (void)hardwareId; return JAVACALL_TRUE; }
int javacall_lcd_get_display_capabilities(int hardwareId) { (void)hardwareId; return 255; }

int* javacall_lcd_get_display_device_ids(int* n) {
    *n = 1;
    return screen_ids;
}

#ifdef __cplusplus
}
#endif
