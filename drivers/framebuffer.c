#include "../include/framebuffer.h"
#include "../include/types.h"

/* ─── State ─── */
Framebuffer fb;
uint32_t backbuf[1024 * 768];

/* ════════════════════════════════════════════
 *  Multiboot2 tag parsing
 * ════════════════════════════════════════════ */
#define MB2_MAGIC        0x36D76289UL
#define TAG_FRAMEBUFFER  8

typedef struct { uint32_t type; uint32_t size; } __attribute__((packed)) MB2Tag;
typedef struct {
    uint32_t type; uint32_t size;
    uint64_t addr; uint32_t pitch; uint32_t width; uint32_t height;
    uint8_t  bpp;  uint8_t  fb_type;
} __attribute__((packed)) MB2TagFB;

void fb_init(uint64_t mbi_ptr) {
    /* First 8 bytes: total_size (u32), reserved (u32) */
    uint32_t total = *(uint32_t*)(mbi_ptr);
    uint8_t *p = (uint8_t*)(mbi_ptr + 8);
    uint8_t *end = (uint8_t*)(mbi_ptr) + total;

    while (p < end) {
        MB2Tag *tag = (MB2Tag*)p;
        if (tag->type == 0 && tag->size == 8) break;
        if (tag->type == TAG_FRAMEBUFFER) {
            MB2TagFB *t = (MB2TagFB*)p;
            fb.vram   = (uint32_t*)(uint64_t)t->addr;
            fb.pitch  = t->pitch;
            fb.width  = t->width;
            fb.height = t->height;
            return;
        }
        /* align to 8 bytes */
        p += (tag->size + 7) & ~7;
    }
    /* Fallback: VGA-compatible address (shouldn't reach here) */
    fb.vram   = (uint32_t*)0xFD000000ULL;
    fb.pitch  = 1024 * 4;
    fb.width  = 1024;
    fb.height = 768;
}

/* ════════════════════════════════════════════
 *  Bitmap Font  (8 × 16, ASCII 32-126)
 *  Compact subset — printable ASCII only
 * ════════════════════════════════════════════ */
static const uint8_t font8x16[95][16] = {
/* 32 space */ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* 33 !    */ {0,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0,0x18,0x18,0,0,0,0,0},
/* 34 "    */ {0,0x66,0x66,0x66,0x24,0,0,0,0,0,0,0,0,0,0,0},
/* 35 #    */ {0,0x36,0x36,0x7f,0x36,0x36,0x36,0x7f,0x36,0x36,0,0,0,0,0,0},
/* 36 $    */ {0x08,0x3e,0x6b,0x68,0x68,0x3e,0x0b,0x0b,0x6b,0x3e,0x08,0,0,0,0,0},
/* 37 %    */ {0,0x60,0x66,0x0c,0x18,0x18,0x30,0x66,0x06,0,0,0,0,0,0,0},
/* 38 &    */ {0,0x38,0x6c,0x6c,0x38,0x76,0xdc,0xcc,0xcc,0x76,0,0,0,0,0,0},
/* 39 '    */ {0,0x18,0x18,0x18,0x10,0,0,0,0,0,0,0,0,0,0,0},
/* 40 (    */ {0,0x0c,0x18,0x30,0x30,0x30,0x30,0x30,0x18,0x0c,0,0,0,0,0,0},
/* 41 )    */ {0,0x30,0x18,0x0c,0x0c,0x0c,0x0c,0x0c,0x18,0x30,0,0,0,0,0,0},
/* 42 *    */ {0,0,0x66,0x3c,0xff,0x3c,0x66,0,0,0,0,0,0,0,0,0},
/* 43 +    */ {0,0,0x18,0x18,0x7e,0x18,0x18,0,0,0,0,0,0,0,0,0},
/* 44 ,    */ {0,0,0,0,0,0,0,0,0x18,0x18,0x08,0x10,0,0,0,0},
/* 45 -    */ {0,0,0,0,0,0x7e,0,0,0,0,0,0,0,0,0,0},
/* 46 .    */ {0,0,0,0,0,0,0,0,0,0x18,0x18,0,0,0,0,0},
/* 47 /    */ {0,0x01,0x02,0x04,0x08,0x10,0x20,0x40,0,0,0,0,0,0,0,0},
/* 48 0    */ {0,0x3c,0x66,0x6e,0x76,0x66,0x66,0x3c,0,0,0,0,0,0,0,0},
/* 49 1    */ {0,0x18,0x38,0x18,0x18,0x18,0x18,0x7e,0,0,0,0,0,0,0,0},
/* 50 2    */ {0,0x3c,0x66,0x06,0x0c,0x18,0x30,0x7e,0,0,0,0,0,0,0,0},
/* 51 3    */ {0,0x7e,0x0c,0x18,0x0c,0x06,0x66,0x3c,0,0,0,0,0,0,0,0},
/* 52 4    */ {0,0x0c,0x1c,0x3c,0x6c,0x7e,0x0c,0x0c,0,0,0,0,0,0,0,0},
/* 53 5    */ {0,0x7e,0x60,0x7c,0x06,0x06,0x66,0x3c,0,0,0,0,0,0,0,0},
/* 54 6    */ {0,0x1c,0x30,0x60,0x7c,0x66,0x66,0x3c,0,0,0,0,0,0,0,0},
/* 55 7    */ {0,0x7e,0x06,0x0c,0x18,0x30,0x30,0x30,0,0,0,0,0,0,0,0},
/* 56 8    */ {0,0x3c,0x66,0x66,0x3c,0x66,0x66,0x3c,0,0,0,0,0,0,0,0},
/* 57 9    */ {0,0x3c,0x66,0x66,0x3e,0x06,0x0c,0x38,0,0,0,0,0,0,0,0},
/* 58 :    */ {0,0,0,0x18,0x18,0,0x18,0x18,0,0,0,0,0,0,0,0},
/* 59 ;    */ {0,0,0,0x18,0x18,0,0x18,0x18,0x08,0x10,0,0,0,0,0,0},
/* 60 <    */ {0,0x06,0x0c,0x18,0x30,0x18,0x0c,0x06,0,0,0,0,0,0,0,0},
/* 61 =    */ {0,0,0,0x7e,0,0x7e,0,0,0,0,0,0,0,0,0,0},
/* 62 >    */ {0,0x60,0x30,0x18,0x0c,0x18,0x30,0x60,0,0,0,0,0,0,0,0},
/* 63 ?    */ {0,0x3c,0x66,0x06,0x0c,0x18,0,0x18,0,0,0,0,0,0,0,0},
/* 64 @    */ {0,0x3c,0x66,0x6e,0x6a,0x6e,0x60,0x3c,0,0,0,0,0,0,0,0},
/* 65 A    */ {0,0x18,0x3c,0x66,0x66,0x7e,0x66,0x66,0,0,0,0,0,0,0,0},
/* 66 B    */ {0,0x7c,0x66,0x66,0x7c,0x66,0x66,0x7c,0,0,0,0,0,0,0,0},
/* 67 C    */ {0,0x3c,0x66,0x60,0x60,0x60,0x66,0x3c,0,0,0,0,0,0,0,0},
/* 68 D    */ {0,0x78,0x6c,0x66,0x66,0x66,0x6c,0x78,0,0,0,0,0,0,0,0},
/* 69 E    */ {0,0x7e,0x60,0x60,0x7c,0x60,0x60,0x7e,0,0,0,0,0,0,0,0},
/* 70 F    */ {0,0x7e,0x60,0x60,0x7c,0x60,0x60,0x60,0,0,0,0,0,0,0,0},
/* 71 G    */ {0,0x3c,0x66,0x60,0x6e,0x66,0x66,0x3c,0,0,0,0,0,0,0,0},
/* 72 H    */ {0,0x66,0x66,0x66,0x7e,0x66,0x66,0x66,0,0,0,0,0,0,0,0},
/* 73 I    */ {0,0x3c,0x18,0x18,0x18,0x18,0x18,0x3c,0,0,0,0,0,0,0,0},
/* 74 J    */ {0,0x1e,0x0c,0x0c,0x0c,0x0c,0x6c,0x38,0,0,0,0,0,0,0,0},
/* 75 K    */ {0,0x66,0x6c,0x78,0x70,0x78,0x6c,0x66,0,0,0,0,0,0,0,0},
/* 76 L    */ {0,0x60,0x60,0x60,0x60,0x60,0x60,0x7e,0,0,0,0,0,0,0,0},
/* 77 M    */ {0,0x63,0x77,0x7f,0x6b,0x63,0x63,0x63,0,0,0,0,0,0,0,0},
/* 78 N    */ {0,0x66,0x76,0x7e,0x7e,0x6e,0x66,0x66,0,0,0,0,0,0,0,0},
/* 79 O    */ {0,0x3c,0x66,0x66,0x66,0x66,0x66,0x3c,0,0,0,0,0,0,0,0},
/* 80 P    */ {0,0x7c,0x66,0x66,0x7c,0x60,0x60,0x60,0,0,0,0,0,0,0,0},
/* 81 Q    */ {0,0x3c,0x66,0x66,0x66,0x6a,0x6c,0x36,0,0,0,0,0,0,0,0},
/* 82 R    */ {0,0x7c,0x66,0x66,0x7c,0x78,0x6c,0x66,0,0,0,0,0,0,0,0},
/* 83 S    */ {0,0x3c,0x66,0x60,0x3c,0x06,0x66,0x3c,0,0,0,0,0,0,0,0},
/* 84 T    */ {0,0x7e,0x18,0x18,0x18,0x18,0x18,0x18,0,0,0,0,0,0,0,0},
/* 85 U    */ {0,0x66,0x66,0x66,0x66,0x66,0x66,0x3c,0,0,0,0,0,0,0,0},
/* 86 V    */ {0,0x66,0x66,0x66,0x66,0x66,0x3c,0x18,0,0,0,0,0,0,0,0},
/* 87 W    */ {0,0x63,0x63,0x63,0x6b,0x7f,0x77,0x63,0,0,0,0,0,0,0,0},
/* 88 X    */ {0,0x66,0x66,0x3c,0x18,0x3c,0x66,0x66,0,0,0,0,0,0,0,0},
/* 89 Y    */ {0,0x66,0x66,0x66,0x3c,0x18,0x18,0x18,0,0,0,0,0,0,0,0},
/* 90 Z    */ {0,0x7e,0x06,0x0c,0x18,0x30,0x60,0x7e,0,0,0,0,0,0,0,0},
/* 91 [    */ {0,0x3c,0x30,0x30,0x30,0x30,0x30,0x3c,0,0,0,0,0,0,0,0},
/* 92 \    */ {0,0x40,0x20,0x10,0x08,0x04,0x02,0x01,0,0,0,0,0,0,0,0},
/* 93 ]    */ {0,0x3c,0x0c,0x0c,0x0c,0x0c,0x0c,0x3c,0,0,0,0,0,0,0,0},
/* 94 ^    */ {0,0x18,0x3c,0x66,0,0,0,0,0,0,0,0,0,0,0,0},
/* 95 _    */ {0,0,0,0,0,0,0,0,0,0,0,0,0x7e,0,0,0},
/* 96 `    */ {0,0x18,0x18,0x0c,0,0,0,0,0,0,0,0,0,0,0,0},
/* 97 a    */ {0,0,0,0x3c,0x06,0x3e,0x66,0x3e,0,0,0,0,0,0,0,0},
/* 98 b    */ {0,0x60,0x60,0x7c,0x66,0x66,0x66,0x7c,0,0,0,0,0,0,0,0},
/* 99 c    */ {0,0,0,0x3c,0x60,0x60,0x60,0x3c,0,0,0,0,0,0,0,0},
/*100 d    */ {0,0x06,0x06,0x3e,0x66,0x66,0x66,0x3e,0,0,0,0,0,0,0,0},
/*101 e    */ {0,0,0,0x3c,0x66,0x7e,0x60,0x3c,0,0,0,0,0,0,0,0},
/*102 f    */ {0,0x1c,0x30,0x30,0x7c,0x30,0x30,0x30,0,0,0,0,0,0,0,0},
/*103 g    */ {0,0,0,0x3e,0x66,0x66,0x3e,0x06,0x06,0x3c,0,0,0,0,0,0},
/*104 h    */ {0,0x60,0x60,0x7c,0x66,0x66,0x66,0x66,0,0,0,0,0,0,0,0},
/*105 i    */ {0,0x18,0,0x38,0x18,0x18,0x18,0x3c,0,0,0,0,0,0,0,0},
/*106 j    */ {0,0x06,0,0x06,0x06,0x06,0x06,0x66,0x66,0x3c,0,0,0,0,0,0},
/*107 k    */ {0,0x60,0x60,0x66,0x6c,0x78,0x6c,0x66,0,0,0,0,0,0,0,0},
/*108 l    */ {0,0x38,0x18,0x18,0x18,0x18,0x18,0x3c,0,0,0,0,0,0,0,0},
/*109 m    */ {0,0,0,0x66,0x7f,0x6b,0x63,0x63,0,0,0,0,0,0,0,0},
/*110 n    */ {0,0,0,0x7c,0x66,0x66,0x66,0x66,0,0,0,0,0,0,0,0},
/*111 o    */ {0,0,0,0x3c,0x66,0x66,0x66,0x3c,0,0,0,0,0,0,0,0},
/*112 p    */ {0,0,0,0x7c,0x66,0x66,0x7c,0x60,0x60,0x60,0,0,0,0,0,0},
/*113 q    */ {0,0,0,0x3e,0x66,0x66,0x3e,0x06,0x06,0x06,0,0,0,0,0,0},
/*114 r    */ {0,0,0,0x6c,0x76,0x60,0x60,0x60,0,0,0,0,0,0,0,0},
/*115 s    */ {0,0,0,0x3c,0x60,0x3c,0x06,0x7c,0,0,0,0,0,0,0,0},
/*116 t    */ {0,0x30,0x30,0x7c,0x30,0x30,0x30,0x1c,0,0,0,0,0,0,0,0},
/*117 u    */ {0,0,0,0x66,0x66,0x66,0x66,0x3e,0,0,0,0,0,0,0,0},
/*118 v    */ {0,0,0,0x66,0x66,0x66,0x3c,0x18,0,0,0,0,0,0,0,0},
/*119 w    */ {0,0,0,0x63,0x63,0x6b,0x7f,0x36,0,0,0,0,0,0,0,0},
/*120 x    */ {0,0,0,0x66,0x3c,0x18,0x3c,0x66,0,0,0,0,0,0,0,0},
/*121 y    */ {0,0,0,0x66,0x66,0x3e,0x06,0x3c,0,0,0,0,0,0,0,0},
/*122 z    */ {0,0,0,0x7e,0x0c,0x18,0x30,0x7e,0,0,0,0,0,0,0,0},
/*123 {    */ {0,0x0e,0x18,0x18,0x70,0x18,0x18,0x0e,0,0,0,0,0,0,0,0},
/*124 |    */ {0,0x18,0x18,0x18,0x00,0x18,0x18,0x18,0,0,0,0,0,0,0,0},
/*125 }    */ {0,0x70,0x18,0x18,0x0e,0x18,0x18,0x70,0,0,0,0,0,0,0,0},
/*126 ~    */ {0,0x76,0xdc,0,0,0,0,0,0,0,0,0,0,0,0,0},
};

/* ════════════════════════════════════════════
 *  Draw Primitives
 * ════════════════════════════════════════════ */

void fb_fill_rect(int x, int y, int w, int h, uint32_t color) {
    int x1 = x + w, y1 = y + h;
    if (x  < 0) x  = 0;
    if (y  < 0) y  = 0;
    if (x1 > (int)fb.width)  x1 = (int)fb.width;
    if (y1 > (int)fb.height) y1 = (int)fb.height;
    for (int row = y; row < y1; row++) {
        uint32_t *p = backbuf + row * (int)fb.width + x;
        for (int col = x; col < x1; col++) *p++ = color;
    }
}

void fb_draw_rect_outline(int x, int y, int w, int h, uint32_t color) {
    fb_fill_rect(x,       y,       w, 1, color);
    fb_fill_rect(x,       y+h-1,   w, 1, color);
    fb_fill_rect(x,       y,       1, h, color);
    fb_fill_rect(x+w-1,   y,       1, h, color);
}

void fb_fill_rect_gradient_v(int x, int y, int w, int h, uint32_t top, uint32_t bot) {
    for (int row = 0; row < h; row++) {
        uint8_t tr = top>>16&0xFF, tg = top>>8&0xFF, tb = top&0xFF;
        uint8_t br = bot>>16&0xFF, bg = bot>>8&0xFF, bb = bot&0xFF;
        uint8_t r = (uint8_t)(tr + (int)(br-tr)*row/h);
        uint8_t g = (uint8_t)(tg + (int)(bg-tg)*row/h);
        uint8_t b = (uint8_t)(tb + (int)(bb-tb)*row/h);
        fb_fill_rect(x, y+row, w, 1, rgb(r,g,b));
    }
}

void fb_fill_rect_gradient_h(int x, int y, int w, int h, uint32_t left, uint32_t right) {
    for (int col = 0; col < w; col++) {
        uint8_t lr = left>>16&0xFF, lg = left>>8&0xFF, lb = left&0xFF;
        uint8_t rr = right>>16&0xFF, rg = right>>8&0xFF, rb = right&0xFF;
        uint8_t r = (uint8_t)(lr + (int)(rr-lr)*col/w);
        uint8_t g = (uint8_t)(lg + (int)(rg-lg)*col/w);
        uint8_t b = (uint8_t)(lb + (int)(rb-lb)*col/w);
        fb_fill_rect(x+col, y, 1, h, rgb(r,g,b));
    }
}

void fb_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg, bool transparent_bg) {
    if (c < 32 || c > 126) c = '?';
    const uint8_t *glyph = font8x16[c - 32];
    for (int row = 0; row < 16; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            int px = x + col, py = y + row;
            if (px < 0 || py < 0 || px >= (int)fb.width || py >= (int)fb.height) continue;
            if (bits & (0x80 >> col))
                backbuf[py * fb.width + px] = fg;
            else if (!transparent_bg)
                backbuf[py * fb.width + px] = bg;
        }
    }
}

void fb_draw_string(int x, int y, const char *s, uint32_t fg, uint32_t bg, bool transparent_bg) {
    int cx = x;
    while (*s) {
        if (*s == '\n') { cx = x; y += 16; }
        else { fb_draw_char(cx, y, *s, fg, bg, transparent_bg); cx += 8; }
        s++;
    }
}

static void swap_int(int *a, int *b) { int t = *a; *a = *b; *b = t; }
static int abs_int(int v) { return v < 0 ? -v : v; }

void fb_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = abs_int(x1-x0), dy = abs_int(y1-y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    while (1) {
        if (x0>=0 && y0>=0 && x0<(int)fb.width && y0<(int)fb.height)
            backbuf[y0*fb.width+x0] = color;
        if (x0==x1 && y0==y1) break;
        int e2 = 2*err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

void fb_fill_circle(int cx, int cy, int r, uint32_t color) {
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x*x + y*y <= r*r)
                if (cx+x>=0 && cy+y>=0 && cx+x<(int)fb.width && cy+y<(int)fb.height)
                    backbuf[(cy+y)*fb.width+(cx+x)] = color;
}

void fb_draw_circle(int cx, int cy, int r, uint32_t color) {
    int x=0, y=r, d=3-2*r;
    while (x<=y) {
        int pts[8][2] = {{cx+x,cy+y},{cx-x,cy+y},{cx+x,cy-y},{cx-x,cy-y},
                         {cx+y,cy+x},{cx-y,cy+x},{cx+y,cy-x},{cx-y,cy-x}};
        for (int i=0;i<8;i++)
            if (pts[i][0]>=0&&pts[i][1]>=0&&pts[i][0]<(int)fb.width&&pts[i][1]<(int)fb.height)
                backbuf[pts[i][1]*fb.width+pts[i][0]]=color;
        if (d<0) d+=4*x+6; else { d+=4*(x-y)+10; y--; }
        x++;
    }
}

/* ════════════════════════════════════════════
 *  Present
 * ════════════════════════════════════════════ */
void fb_present(void) {
    uint32_t *src = backbuf;
    uint8_t  *dst = (uint8_t*)fb.vram;
    uint32_t rows = fb.height, cols = fb.width;
    for (uint32_t row = 0; row < rows; row++) {
        uint64_t *d = (uint64_t*)(dst + row * fb.pitch);
        uint64_t *s = (uint64_t*)(src + row * cols);
        for (uint32_t c = 0; c < cols/2; c++)
            *d++ = *s++;
    }
}
