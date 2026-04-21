#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "types.h"

/* ─── Framebuffer state ─── */
typedef struct {
    uint32_t *vram;      /* VRAM base address from Multiboot2 */
    uint32_t  width;
    uint32_t  height;
    uint32_t  pitch;     /* bytes per row */
} Framebuffer;

extern Framebuffer fb;

/* ─── Back-buffer (3 MB static) ─── */
extern uint32_t backbuf[1024 * 768];

/* ─── Init ─── */
void fb_init(uint64_t multiboot_info_ptr);

/* ─── Draw primitives ─── */
void fb_fill_rect(int x, int y, int w, int h, uint32_t color);
void fb_draw_rect_outline(int x, int y, int w, int h, uint32_t color);
void fb_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg, bool transparent_bg);
void fb_draw_string(int x, int y, const char *s, uint32_t fg, uint32_t bg, bool transparent_bg);
void fb_draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void fb_draw_circle(int cx, int cy, int r, uint32_t color);
void fb_fill_circle(int cx, int cy, int r, uint32_t color);

/* Gradient helpers */
void fb_fill_rect_gradient_v(int x, int y, int w, int h, uint32_t top, uint32_t bottom);
void fb_fill_rect_gradient_h(int x, int y, int w, int h, uint32_t left, uint32_t right);

/* ─── Present back-buffer → VRAM ─── */
void fb_present(void);

/* ─── Colour helpers ─── */
static inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}
static inline uint32_t blend(uint32_t fg, uint32_t bg, uint8_t alpha) {
    uint8_t r = ((fg>>16&0xFF)*alpha + (bg>>16&0xFF)*(255-alpha)) / 255;
    uint8_t g = ((fg>> 8&0xFF)*alpha + (bg>> 8&0xFF)*(255-alpha)) / 255;
    uint8_t b = ((fg    &0xFF)*alpha + (bg    &0xFF)*(255-alpha)) / 255;
    return rgb(r,g,b);
}

/* Lighten / darken a colour */
static inline uint32_t lighten(uint32_t c, uint8_t amt) {
    uint8_t r = (c>>16&0xFF); uint8_t g = (c>>8&0xFF); uint8_t b = (c&0xFF);
    r = (r+amt>255)?255:r+amt; g = (g+amt>255)?255:g+amt; b = (b+amt>255)?255:b+amt;
    return rgb(r,g,b);
}
static inline uint32_t darken(uint32_t c, uint8_t amt) {
    uint8_t r = (c>>16&0xFF); uint8_t g = (c>>8&0xFF); uint8_t b = (c&0xFF);
    r = (r<amt)?0:r-amt; g = (g<amt)?0:g-amt; b = (b<amt)?0:b-amt;
    return rgb(r,g,b);
}

#endif
