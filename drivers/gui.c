#include "../include/gui.h"
#include "../include/framebuffer.h"
#include "../include/types.h"

/* ════════════════════════════════════════════
 *  Global State
 * ════════════════════════════════════════════ */
static Window    windows[MAX_WINDOWS];
static DeskIcon  icons[MAX_ICONS];
static int       icon_count = 0;
static Mouse     mouse;
static Terminal  term;
static int       focused_win = -1;
static int       frame_count = 0;

/* tiny string helpers (no libc) */
static int str_len(const char *s) { int n=0; while(s[n]) n++; return n; }
static void str_cpy(char *d, const char *s) { while((*d++=*s++)); }
static void str_cat(char *d, const char *s) { d+=str_len(d); while((*d++=*s++)); }
static int  str_eq(const char *a, const char *b) { while(*a&&*b&&*a==*b){a++;b++;} return *a==*b; }
static void int_to_str(int v, char *buf) {
    if (v==0){buf[0]='0';buf[1]=0;return;}
    char tmp[12]; int i=0;
    if(v<0){*buf++='-';v=-v;}
    while(v){tmp[i++]='0'+v%10;v/=10;}
    for(int j=i-1;j>=0;j--)*buf++=tmp[j];
    *buf=0;
}

/* ════════════════════════════════════════════
 *  Desktop wallpaper  (Luna-style gradient + dot-grid)
 * ════════════════════════════════════════════ */
static void draw_wallpaper(void) {
    /* Vertical gradient sky */
    fb_fill_rect_gradient_v(0, 0, 1024, 768, 0x3A7BD5, 0x0E4CAD);

    /* Subtle dot-grid pattern */
    for (int y = 20; y < 768 - TASKBAR_H; y += 24)
        for (int x = 20; x < 1024; x += 24)
            fb_fill_rect(x, y, 2, 2, 0x2A6BC5);

    /* Horizon glow band */
    fb_fill_rect_gradient_v(0, 580, 1024, 80, 0x5AB0F0, 0x1060C8);
    fb_fill_rect_gradient_v(0, 660, 1024, 74, 0x2A5EAB, 0x0E3A8A);

    /* Bottom green strip (XP meadow) */
    fb_fill_rect_gradient_v(0, 700, 1024, 34, 0x4CAF50, 0x2E7D32);
}

/* ════════════════════════════════════════════
 *  XP-Style Window Chrome
 * ════════════════════════════════════════════ */
static void draw_window(int idx) {
    Window *w = &windows[idx];
    if (!w->open || w->minimised) return;

    int x=w->x, y=w->y, ww=w->w, wh=w->h;
    bool active = (idx == focused_win);

    /* Drop shadow */
    fb_fill_rect(x+4, y+4, ww, wh, 0x000000);

    /* Outer border — 3D frame */
    fb_fill_rect(x, y, ww, wh, COL_BTN_SHADOW);
    fb_fill_rect(x+1, y+1, ww-2, wh-2, COL_BTN_FACE);
    /* Highlight edges (top-left) */
    fb_fill_rect(x+1, y+1, ww-2, 1, COL_BTN_HILIGHT);
    fb_fill_rect(x+1, y+1, 1, wh-2, COL_BTN_HILIGHT);
    /* Dark edges (bottom-right) */
    fb_fill_rect(x+1, y+wh-2, ww-2, 1, COL_BTN_DKSHADOW);
    fb_fill_rect(x+ww-2, y+1, 1, wh-2, COL_BTN_DKSHADOW);

    /* Title bar gradient */
    if (active) {
        /* Luna blue multi-stop gradient */
        fb_fill_rect_gradient_v(x+3, y+3, ww-6, TITLEBAR_H/2,
            0x2458CF, 0x3A7FE0);
        fb_fill_rect_gradient_v(x+3, y+3+TITLEBAR_H/2, ww-6, TITLEBAR_H/2,
            0x2060D5, 0x0A246A);
    } else {
        fb_fill_rect_gradient_v(x+3, y+3, ww-6, TITLEBAR_H,
            0x7191BC, 0x5178A8);
    }

    /* Thin colour strip along top of titlebar */
    fb_fill_rect(x+3, y+3, ww-6, 2, active ? 0x5B9DFF : 0x9AB3CE);

    /* Title text */
    int tx = x + 28;
    int ty = y + 3 + (TITLEBAR_H - 16) / 2;
    /* Drop shadow for text */
    fb_draw_string(tx+1, ty+1, w->title, 0x000000, 0, true);
    fb_draw_string(tx, ty, w->title, active ? COL_WHITE : 0xC0C0C0, 0, true);

    /* XP window icon (small coloured square) */
    fb_fill_rect(x+6, y+6, 16, 16, 0x2060D5);
    fb_fill_rect(x+7, y+7, 14, 14, 0x4080FF);
    fb_fill_rect(x+10, y+10, 8, 8, 0xFFFFFF);

    /* ── Traffic-light buttons ── */
    /* Close button (red) */
    int bx = x + ww - 21;
    int by = y + 4;
    int bw = 17, bh = TITLEBAR_H - 8;

    /* Close */
    fb_fill_rect(bx, by, bw, bh, 0xCC3737);
    fb_fill_rect_gradient_v(bx, by, bw, bh/2, 0xFF6060, 0xCC3737);
    fb_fill_rect(bx+1, by+1, bw-2, 1, 0xFF9090);
    /* X mark */
    fb_draw_line(bx+4, by+4, bx+bw-5, by+bh-5, COL_WHITE);
    fb_draw_line(bx+bw-5, by+4, bx+4, by+bh-5, COL_WHITE);

    /* Maximise button (green) */
    bx -= bw + 2;
    fb_fill_rect(bx, by, bw, bh, 0x337733);
    fb_fill_rect_gradient_v(bx, by, bw, bh/2, 0x66BB66, 0x337733);
    fb_fill_rect(bx+1, by+1, bw-2, 1, 0x99EE99);
    /* □ mark */
    fb_draw_rect_outline(bx+4, by+4, bw-8, bh-8, COL_WHITE);

    /* Minimise button (yellow/amber) */
    bx -= bw + 2;
    fb_fill_rect(bx, by, bw, bh, 0xAA8800);
    fb_fill_rect_gradient_v(bx, by, bw, bh/2, 0xFFCC00, 0xAA8800);
    fb_fill_rect(bx+1, by+1, bw-2, 1, 0xFFEE88);
    /* − mark */
    fb_fill_rect(bx+4, by+bh/2-1, bw-8, 2, COL_WHITE);

    /* ── Window border line under titlebar ── */
    fb_fill_rect(x+BORDER_W, y+3+TITLEBAR_H, ww-BORDER_W*2, 1, active?0x0A246A:0x5178A8);

    /* ── Client area ── */
    fb_fill_rect(x+BORDER_W, y+3+TITLEBAR_H+1, ww-BORDER_W*2, wh-3-TITLEBAR_H-BORDER_W-1,
        COL_WINDOW_BG);
}

/* Scrollbar (decorative, right side of client area) */
static void draw_scrollbar(int wx, int wy, int ww, int wh, int content_frac_num, int content_frac_den) {
    int sx = wx + ww - BORDER_W - 16;
    int sy = wy + 3 + TITLEBAR_H + 1;
    int sh = wh - 3 - TITLEBAR_H - BORDER_W - 1;

    fb_fill_rect(sx, sy, 16, sh, COL_BTN_FACE);
    fb_fill_rect(sx, sy, 16, 1, COL_BTN_SHADOW);
    fb_fill_rect(sx, sy, 1, sh, COL_BTN_SHADOW);

    /* Up arrow button */
    fb_fill_rect(sx, sy, 16, 16, COL_BTN_FACE);
    fb_draw_line(sx+8, sy+4, sx+4, sy+12, COL_BLACK);
    fb_draw_line(sx+8, sy+4, sx+12, sy+12, COL_BLACK);

    /* Down arrow button */
    fb_fill_rect(sx, sy+sh-16, 16, 16, COL_BTN_FACE);
    fb_draw_line(sx+8, sy+sh-4, sx+4, sy+sh-12, COL_BLACK);
    fb_draw_line(sx+8, sy+sh-4, sx+12, sy+sh-12, COL_BLACK);

    /* Thumb */
    int track_h = sh - 32;
    int thumb_h = track_h * content_frac_num / (content_frac_den > 0 ? content_frac_den : 1);
    if (thumb_h < 20) thumb_h = 20;
    fb_fill_rect(sx+1, sy+17, 14, thumb_h, COL_BTN_FACE);
    fb_fill_rect(sx+1, sy+17, 14, 1, COL_BTN_HILIGHT);
    fb_fill_rect(sx+1, sy+17, 1, thumb_h, COL_BTN_HILIGHT);
    fb_fill_rect(sx+14, sy+17, 1, thumb_h, COL_BTN_SHADOW);
    fb_fill_rect(sx+1, sy+17+thumb_h-1, 14, 1, COL_BTN_SHADOW);
    (void)track_h;
}

/* ════════════════════════════════════════════
 *  Taskbar
 * ════════════════════════════════════════════ */
static void draw_taskbar(void) {
    int tbY = 768 - TASKBAR_H;

    /* Gradient body */
    fb_fill_rect_gradient_v(0, tbY, 1024, TASKBAR_H, COL_TASKBAR_TOP, COL_TASKBAR_BOT);

    /* Top highlight line */
    fb_fill_rect(0, tbY, 1024, 1, 0x5BA0F0);
    fb_fill_rect(0, tbY+1, 1024, 1, 0x3070C0);

    /* ── Start Button ── */
    int sbW = 82, sbH = 28;
    int sbX = 4, sbY = tbY + (TASKBAR_H - sbH) / 2;

    /* Outer rounded rect (simulated with corners) */
    fb_fill_rect(sbX+2, sbY, sbW-4, sbH, COL_START_BTN_BOT);
    fb_fill_rect(sbX, sbY+2, sbW, sbH-4, COL_START_BTN_BOT);
    fb_fill_rect_gradient_v(sbX+2, sbY, sbW-4, sbH/2, 0x7ADE7A, COL_START_BTN_TOP);
    fb_fill_rect_gradient_v(sbX+2, sbY+sbH/2, sbW-4, sbH/2, COL_START_BTN_TOP, 0x1A4A1A);
    /* Highlight top edge */
    fb_fill_rect(sbX+2, sbY, sbW-4, 1, 0xAAFFAA);
    /* Border */
    fb_draw_rect_outline(sbX, sbY+2, sbW, sbH-4, 0x1A5A1A);

    /* Windows logo (4-coloured squares) */
    int lx = sbX+5, ly = sbY+6;
    fb_fill_rect(lx,   ly,   7, 7, 0xFF4444);   /* red  (top-left)  */
    fb_fill_rect(lx+8, ly,   7, 7, 0x44AA44);   /* green (top-right) */
    fb_fill_rect(lx,   ly+8, 7, 7, 0x4444FF);   /* blue (bot-left)  */
    fb_fill_rect(lx+8, ly+8, 7, 7, 0xFFDD00);   /* yellow (bot-right) */

    /* "start" text */
    fb_draw_string(sbX+22, sbY+7, "start", COL_WHITE, 0, true);

    /* ── Window buttons in taskbar ── */
    int bx = sbX + sbW + 8;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        Window *w = &windows[i];
        if (!w->open) continue;
        int bw = 120, bh = 22;
        int by2 = tbY + (TASKBAR_H - bh) / 2;
        if (i == focused_win && !w->minimised) {
            /* Pressed / active */
            fb_fill_rect(bx, by2, bw, bh, COL_BTN_SHADOW);
            fb_fill_rect(bx+1, by2+1, bw-1, bh-1, COL_BTN_FACE);
            fb_fill_rect(bx+1, by2+1, bw-1, 1, COL_BTN_SHADOW);
        } else {
            fb_fill_rect(bx, by2, bw, bh, COL_BTN_FACE);
            fb_fill_rect(bx, by2, bw, 1, COL_BTN_HILIGHT);
            fb_fill_rect(bx, by2, 1, bh, COL_BTN_HILIGHT);
            fb_fill_rect(bx+bw-1, by2, 1, bh, COL_BTN_SHADOW);
            fb_fill_rect(bx, by2+bh-1, bw, 1, COL_BTN_SHADOW);
        }
        /* Truncate title */
        char label[16]; int ti=0;
        while (w->title[ti] && ti<13) { label[ti]=w->title[ti]; ti++; }
        if (w->title[ti]) { label[ti++]='.'; label[ti++]='.'; label[ti++]='.'; }
        label[ti]=0;
        fb_draw_string(bx+4, by2+3, label, COL_BLACK, 0, true);
        bx += bw + 4;
    }

    /* ── System Tray (clock) ── */
    fb_fill_rect(1024-80, tbY+2, 78, TASKBAR_H-4, 0x1A50B0);
    fb_draw_rect_outline(1024-80, tbY+2, 78, TASKBAR_H-4, 0x0A3090);
    /* Static clock (no RTC — shows HiuraOS boot time) */
    fb_draw_string(1024-68, tbY+9, "00:00:00", COL_WHITE, 0, true);
}

/* ════════════════════════════════════════════
 *  Desktop Icons
 * ════════════════════════════════════════════ */
static void add_icon(int x, int y, uint32_t color, const char *label, WinType target) {
    if (icon_count >= MAX_ICONS) return;
    DeskIcon *ic = &icons[icon_count++];
    ic->x = x; ic->y = y;
    ic->icon_color = color;
    str_cpy(ic->label, label);
    ic->target = target;
}

static bool point_in(int px, int py, int rx, int ry, int rw, int rh) {
    return px>=rx && py>=ry && px<rx+rw && py<ry+rh;
}

static void draw_icon(DeskIcon *ic) {
    bool hover = point_in(mouse.x, mouse.y, ic->x-4, ic->y-4, ICON_W+8, ICON_H+20);

    /* Hover highlight */
    if (hover)
        fb_fill_rect(ic->x-4, ic->y-4, ICON_W+8, ICON_H+24, 0x316AC540);

    /* Icon body — folder-like shape */
    fb_fill_rect(ic->x, ic->y, ICON_W, ICON_H-8, ic->icon_color);
    fb_fill_rect(ic->x, ic->y+4, ICON_W, ICON_H-8, darken(ic->icon_color, 20));
    /* Tab on top */
    fb_fill_rect(ic->x+2, ic->y, 16, 6, lighten(ic->icon_color, 40));
    /* Shine */
    fb_fill_rect(ic->x+2, ic->y+5, ICON_W-4, 4, lighten(ic->icon_color, 60));
    /* Border */
    fb_draw_rect_outline(ic->x, ic->y, ICON_W, ICON_H-4, darken(ic->icon_color, 60));

    /* Label (with selection box) */
    int lw = str_len(ic->label) * 8;
    int lx = ic->x + (ICON_W - lw) / 2;
    int ly = ic->y + ICON_H - 2;
    if (hover)
        fb_fill_rect(lx-2, ly-1, lw+4, 18, COL_SELECTION);
    fb_draw_string(lx, ly, ic->label, COL_WHITE, 0, true);
}

/* ════════════════════════════════════════════
 *  Terminal Window Content
 * ════════════════════════════════════════════ */
static void term_push_line(const char *line) {
    int idx = (term.top + term.count) % TERM_HISTORY;
    int i = 0;
    while (line[i] && i < TERM_COLS) { term.lines[idx][i] = line[i]; i++; }
    term.lines[idx][i] = 0;
    if (term.count < TERM_HISTORY) term.count++;
    else term.top = (term.top + 1) % TERM_HISTORY;
}

static void term_run_cmd(void) {
    char *cmd = term.input;
    char echo_line[TERM_COLS+4]; echo_line[0]='>'; echo_line[1]=' '; str_cpy(echo_line+2, cmd);
    term_push_line(echo_line);

    if (str_eq(cmd,"help")) {
        term_push_line("Commands: help, clear, uname, about, ls, echo <text>");
    } else if (str_eq(cmd,"clear")) {
        term.count = 0; term.top = 0;
    } else if (str_eq(cmd,"uname") || str_eq(cmd,"about")) {
        term_push_line("HiuraOS v0.1 x86_64 (Long Mode)");
        term_push_line("Build: NASM + GCC, no libc");
    } else if (str_eq(cmd,"ls")) {
        term_push_line("boot/   drivers/   include/");
        term_push_line("kernel/ linker.ld  Makefile");
        term_push_line("grub.cfg  README.md");
    } else if (cmd[0]=='e' && cmd[1]=='c' && cmd[2]=='h' && cmd[3]=='o' && cmd[4]==' ') {
        term_push_line(cmd+5);
    } else if (cmd[0]) {
        char err[64]; str_cpy(err,"Unknown command: "); str_cat(err,cmd);
        term_push_line(err);
    }

    term.input[0] = 0; term.input_len = 0;
}

static void draw_terminal_content(Window *w) {
    int cx = w->x + BORDER_W;
    int cy = w->y + 3 + TITLEBAR_H + 1;
    int cw = w->w - BORDER_W*2 - 16; /* leave room for scrollbar */
    int ch = w->h - 3 - TITLEBAR_H - BORDER_W - 1;

    /* Background — classic terminal black */
    fb_fill_rect(cx, cy, w->w - BORDER_W*2, ch, 0x0C0C0C);
    fb_fill_rect(cx, cy, cw, ch, 0x0C0C0C);

    /* Scanline effect (subtle) */
    for (int row = cy+1; row < cy+ch; row += 2)
        fb_fill_rect(cx, row, cw, 1, 0x111111);

    /* Render terminal lines (visible portion) */
    int visible = (ch - 20) / 16; /* how many lines fit */
    if (visible > TERM_ROWS) visible = TERM_ROWS;
    int start = term.count > visible ? term.count - visible : 0;

    for (int i = 0; i < visible; i++) {
        int li = (term.top + start + i) % TERM_HISTORY;
        int ly2 = cy + 4 + i * 16;
        if (ly2 + 16 >= cy + ch - 20) break;
        const char *line = term.lines[li];
        uint32_t color = 0xC8C8C8;
        if (line[0]=='>') color = 0x22DD22; /* command echo green */
        fb_draw_string(cx+6, ly2, line, color, 0, true);
    }

    /* Prompt + input line */
    int py = cy + ch - 20;
    fb_draw_string(cx+6, py, "C:\\> ", 0x22DD22, 0, true);
    fb_draw_string(cx+46, py, term.input, 0xFFFFFF, 0, true);

    /* Blinking cursor */
    if (term.cursor_on) {
        int cur_x = cx + 46 + term.input_len * 8;
        fb_fill_rect(cur_x, py, 8, 14, 0xFFFFFF);
    }

    /* Scrollbar */
    draw_scrollbar(w->x, w->y, w->w, w->h, visible, term.count>0?term.count:1);
}

/* ════════════════════════════════════════════
 *  About Window Content
 * ════════════════════════════════════════════ */
static void draw_about_content(Window *w) {
    int cx = w->x + BORDER_W + 8;
    int cy = w->y + 3 + TITLEBAR_H + 8;

    /* HiuraOS logo box */
    fb_fill_rect_gradient_v(w->x+BORDER_W, w->y+3+TITLEBAR_H+1,
        w->w-BORDER_W*2, 80, 0x0A246A, 0x1A5FD0);

    /* Windows-flag-like logo */
    int lx = cx+8, ly = cy+4;
    fb_fill_rect(lx,    ly,    28, 28, 0xFF4444);
    fb_fill_rect(lx+32, ly,    28, 28, 0x44BB44);
    fb_fill_rect(lx,    ly+32, 28, 28, 0x4444FF);
    fb_fill_rect(lx+32, ly+32, 28, 28, 0xFFDD00);

    fb_draw_string(lx+72, ly+6,  "HiuraOS", COL_WHITE, 0, true);
    fb_draw_string(lx+72, ly+22, "Version 0.1.0", 0xCCCCCC, 0, true);

    cy = w->y + 3 + TITLEBAR_H + 90;
    fb_draw_string(cx, cy,    "Architecture:  x86_64 Intel/AMD", COL_BLACK, 0, true);
    cy += 20;
    fb_draw_string(cx, cy,    "Boot:          GRUB2 / Multiboot2", COL_BLACK, 0, true);
    cy += 20;
    fb_draw_string(cx, cy,    "Display:       VESA 1024x768x32", COL_BLACK, 0, true);
    cy += 20;
    fb_draw_string(cx, cy,    "Input:         PS/2 Keyboard+Mouse", COL_BLACK, 0, true);
    cy += 20;
    fb_draw_string(cx, cy,    "Libraries:     None (freestanding)", COL_BLACK, 0, true);
    cy += 28;
    fb_draw_string(cx, cy,    "Built with NASM + GCC, no libc.", 0x555555, 0, true);
    cy += 18;
    fb_draw_string(cx, cy,    "Released into the public domain.", 0x555555, 0, true);
}

/* ════════════════════════════════════════════
 *  Files Window Content
 * ════════════════════════════════════════════ */
static const char *file_icons[] = { "boot", "drivers", "include", "kernel",
                                     "linker.ld", "Makefile" };
static const uint32_t file_colors[] = { 0xF0A020, 0x20A0E0, 0xE04040,
                                         0x40D040, 0x8080FF, 0x808080 };

static void draw_files_content(Window *w) {
    int cx = w->x + BORDER_W;
    int cy = w->y + 3 + TITLEBAR_H + 1;
    int cw = w->w - BORDER_W*2;
    int ch = w->h - 3 - TITLEBAR_H - BORDER_W - 1;

    /* Toolbar */
    fb_fill_rect_gradient_v(cx, cy, cw, 24, COL_BTN_FACE, 0xD0CEC0);
    fb_fill_rect(cx, cy+23, cw, 1, COL_BTN_SHADOW);
    fb_draw_string(cx+6, cy+5, "File   Edit   View   Help", COL_BLACK, 0, true);

    /* Address bar */
    fb_fill_rect(cx, cy+24, cw, 22, COL_WHITE);
    fb_draw_rect_outline(cx, cy+24, cw, 22, COL_BTN_SHADOW);
    fb_draw_string(cx+6, cy+30, "C:\\HiuraOS\\", COL_BLACK, 0, true);
    fb_fill_rect(cx, cy+46, cw, 1, COL_BTN_SHADOW);

    /* File grid */
    int startY = cy + 48;
    int cols_per_row = 4;
    int cell_w = (cw - 16) / cols_per_row;
    int cell_h = 70;

    for (int i = 0; i < 6; i++) {
        int col = i % cols_per_row;
        int row = i / cols_per_row;
        int fx = cx + col * cell_w + 8;
        int fy = startY + row * cell_h + 4;

        bool hover = point_in(mouse.x, mouse.y, fx, fy, cell_w-4, cell_h-4);
        if (hover) fb_fill_rect(fx, fy, cell_w-4, cell_h-4, 0xCCDDFF);

        /* Icon */
        fb_fill_rect(fx+8, fy+4, 36, 28, file_colors[i]);
        fb_fill_rect(fx+8, fy+4, 16, 6, lighten(file_colors[i], 40));
        fb_fill_rect(fx+9, fy+5, 34, 3, lighten(file_colors[i], 80));
        fb_draw_rect_outline(fx+8, fy+4, 36, 28, darken(file_colors[i], 60));

        /* Label */
        int lw = str_len(file_icons[i]) * 8;
        fb_draw_string(fx + (cell_w - lw) / 2 - 8, fy+36, file_icons[i], COL_BLACK, 0, true);
    }

    /* Status bar */
    fb_fill_rect(cx, cy+ch-20, cw, 20, COL_BTN_FACE);
    fb_fill_rect(cx, cy+ch-20, cw, 1, COL_BTN_SHADOW);
    fb_draw_string(cx+6, cy+ch-14, "6 objects", 0x444444, 0, true);
}

/* ════════════════════════════════════════════
 *  Mouse Cursor  (XP-style arrow)
 * ════════════════════════════════════════════ */
static void draw_cursor(void) {
    int x = mouse.x, y = mouse.y;
    /* Arrow shape using filled triangles (manual pixels) */
    static const int arrow[16][2] = {
        {0,0},{0,1},{0,2},{0,3},{0,4},{0,5},{0,6},{0,7},
        {0,8},{0,9},{0,10},{1,1},{1,2},{1,3},{1,4},{1,5}
    };
    /* Outline (black) */
    for (int i=0;i<11;i++) {
        int ax=x+arrow[i][0], ay=y+arrow[i][1];
        if (ax>=0&&ay>=0&&ax<1024&&ay<768) backbuf[ay*1024+ax]=COL_BLACK;
        /* shadow offset */
        if (ax+1>=0&&ay+1>=0&&ax+1<1024&&ay+1<768) backbuf[(ay+1)*1024+(ax+1)]=COL_BLACK;
    }
    /* Fill rows for solid arrow */
    for (int row=0;row<=10;row++) {
        int fill_end = 10 - row;
        for (int col=1; col<=fill_end; col++) {
            int ax=x+col, ay=y+row;
            if (ax>=0&&ay>=0&&ax<1024&&ay<768) backbuf[ay*1024+ax]=COL_WHITE;
        }
    }
    /* Left edge (black) */
    for (int row=0;row<=12;row++) {
        if (x>=0&&y+row>=0&&x<1024&&y+row<768) backbuf[(y+row)*1024+x]=COL_BLACK;
    }
    /* Diagonal (black) */
    for (int d=0;d<=10;d++) {
        int ax=x+d, ay=y+10-d;
        if (ax>=0&&ay>=0&&ax<1024&&ay<768) backbuf[ay*1024+ax]=COL_BLACK;
    }
}

/* ════════════════════════════════════════════
 *  Window Management Helpers
 * ════════════════════════════════════════════ */
static int find_free_window(void) {
    for (int i=0;i<MAX_WINDOWS;i++) if (!windows[i].open) return i;
    return -1;
}

static void open_window(WinType type) {
    /* If already open, focus it */
    for (int i=0;i<MAX_WINDOWS;i++) {
        if (windows[i].open && windows[i].type==type) {
            windows[i].minimised = false;
            focused_win = i;
            return;
        }
    }
    int idx = find_free_window();
    if (idx<0) return;
    Window *w = &windows[idx];
    w->open = true; w->minimised = false; w->maximised = false;
    w->type = type;
    w->dragging = false;

    switch(type) {
        case WIN_TERMINAL:
            w->x=100; w->y=80; w->w=600; w->h=380;
            str_cpy(w->title, "Command Prompt");
            break;
        case WIN_ABOUT:
            w->x=280; w->y=150; w->w=440; w->h=340;
            str_cpy(w->title, "About HiuraOS");
            break;
        case WIN_FILES:
            w->x=160; w->y=60; w->w=580; w->h=400;
            str_cpy(w->title, "My Computer");
            break;
        default:
            w->x=200; w->y=200; w->w=400; w->h=300;
            str_cpy(w->title, "Window");
            break;
    }
    w->restore_x=w->x; w->restore_y=w->y;
    w->restore_w=w->w; w->restore_h=w->h;
    focused_win = idx;
}

/* Titlebar button hit detection */
#define BTN_CLOSE 1
#define BTN_MAX   2
#define BTN_MIN   3

static int titlebar_hit(Window *w, int mx, int my) {
    if (!point_in(mx, my, w->x, w->y+3, w->w, TITLEBAR_H)) return 0;
    int bw=17, bh=TITLEBAR_H-8, by=w->y+4;
    int bx_close = w->x+w->w-21;
    int bx_max   = bx_close - bw - 2;
    int bx_min   = bx_max - bw - 2;
    if (point_in(mx,my,bx_close,by,bw,bh)) return BTN_CLOSE;
    if (point_in(mx,my,bx_max,  by,bw,bh)) return BTN_MAX;
    if (point_in(mx,my,bx_min,  by,bw,bh)) return BTN_MIN;
    return -1; /* title bar drag zone */
}

/* ════════════════════════════════════════════
 *  GUI Init
 * ════════════════════════════════════════════ */
void gui_init(void) {
    for (int i=0;i<MAX_WINDOWS;i++) windows[i].open=false;
    for (int i=0;i<MAX_ICONS;i++) icons[i].x=0;
    icon_count = 0;
    mouse.x = 512; mouse.y = 384;
    mouse.left = mouse.right = false;
    focused_win = -1;
    term.count = 0; term.top = 0;
    term.input[0] = 0; term.input_len = 0;
    term.blink = 0; term.cursor_on = true;

    /* Terminal greeting */
    term_push_line("HiuraOS Command Prompt");
    term_push_line("Type 'help' for available commands.");
    term_push_line("");

    /* Desktop icons */
    add_icon(20,  40, 0xC0A020, "My Computer",    WIN_FILES);
    add_icon(20, 130, 0x2060C0, "Terminal",        WIN_TERMINAL);
    add_icon(20, 220, 0xC04040, "About HiuraOS",   WIN_ABOUT);

    /* Open welcome window */
    open_window(WIN_ABOUT);
}

/* ════════════════════════════════════════════
 *  GUI Render  (called every frame)
 * ════════════════════════════════════════════ */
void gui_render(void) {
    frame_count++;

    /* Blink cursor every ~30 frames */
    term.blink++;
    if (term.blink >= 30) { term.blink=0; term.cursor_on = !term.cursor_on; }

    /* 1. Wallpaper */
    draw_wallpaper();

    /* 2. Desktop icons */
    for (int i=0;i<icon_count;i++) draw_icon(&icons[i]);

    /* 3. Windows (back to front, focused last) */
    for (int i=0;i<MAX_WINDOWS;i++) {
        if (i==focused_win) continue;
        if (!windows[i].open || windows[i].minimised) continue;
        windows[i].focused = false;
        draw_window(i);
        switch (windows[i].type) {
            case WIN_TERMINAL: draw_terminal_content(&windows[i]); break;
            case WIN_ABOUT:    draw_about_content(&windows[i]);    break;
            case WIN_FILES:    draw_files_content(&windows[i]);    break;
            default: break;
        }
    }
    if (focused_win>=0 && windows[focused_win].open && !windows[focused_win].minimised) {
        windows[focused_win].focused = true;
        draw_window(focused_win);
        switch (windows[focused_win].type) {
            case WIN_TERMINAL: draw_terminal_content(&windows[focused_win]); break;
            case WIN_ABOUT:    draw_about_content(&windows[focused_win]);    break;
            case WIN_FILES:    draw_files_content(&windows[focused_win]);    break;
            default: break;
        }
    }

    /* 4. Taskbar (always on top) */
    draw_taskbar();

    /* 5. Mouse cursor */
    draw_cursor();

    /* 6. Flip */
    fb_present();
}

/* ════════════════════════════════════════════
 *  Input: Mouse
 * ════════════════════════════════════════════ */
void gui_mouse_update(int dx, int dy, bool left, bool right) {
    mouse.left_prev = mouse.left;
    mouse.x += dx; mouse.y += dy;
    if (mouse.x < 0) mouse.x=0;
    if (mouse.y < 0) mouse.y=0;
    if (mouse.x >= 1024) mouse.x=1023;
    if (mouse.y >= 768)  mouse.y=767;
    mouse.left  = left;
    mouse.right = right;

    bool just_pressed  = left && !mouse.left_prev;
    bool just_released = !left && mouse.left_prev;

    /* Handle dragging */
    if (mouse.left && focused_win>=0 && windows[focused_win].dragging) {
        Window *w = &windows[focused_win];
        w->x = mouse.x - w->drag_ox;
        w->y = mouse.y - w->drag_oy;
        /* Clamp */
        if (w->x < 0) w->x=0;
        if (w->y < 0) w->y=0;
        if (w->x+w->w > 1024) w->x=1024-w->w;
        if (w->y+w->h > 768-TASKBAR_H) w->y=768-TASKBAR_H-w->h;
        return;
    }

    if (just_released && focused_win>=0)
        windows[focused_win].dragging = false;

    if (!just_pressed) return;

    /* Taskbar window buttons */
    int tbY = 768 - TASKBAR_H;
    if (mouse.y >= tbY) {
        /* Start button area — open files */
        if (point_in(mouse.x, mouse.y, 4, tbY+3, 82, 28)) {
            open_window(WIN_FILES);
            return;
        }
        /* Window buttons */
        int bx = 90;
        for (int i=0;i<MAX_WINDOWS;i++) {
            Window *w = &windows[i];
            if (!w->open) continue;
            int bw=120, bh=22, by2=tbY+(TASKBAR_H-bh)/2;
            if (point_in(mouse.x, mouse.y, bx, by2, bw, bh)) {
                if (i==focused_win && !w->minimised) { w->minimised=true; focused_win=-1; }
                else { w->minimised=false; focused_win=i; }
                return;
            }
            bx += bw+4;
        }
        return;
    }

    /* Desktop icons */
    for (int i=0;i<icon_count;i++) {
        DeskIcon *ic = &icons[i];
        if (point_in(mouse.x, mouse.y, ic->x-4, ic->y-4, ICON_W+8, ICON_H+20)) {
            open_window(ic->target);
            return;
        }
    }

    /* Windows — front to back */
    for (int i=MAX_WINDOWS-1;i>=0;i--) {
        Window *w = &windows[i];
        if (!w->open || w->minimised) continue;
        if (!point_in(mouse.x, mouse.y, w->x, w->y, w->w, w->h)) continue;

        /* Focus this window */
        focused_win = i;

        /* Check titlebar buttons */
        int btn = titlebar_hit(w, mouse.x, mouse.y);
        if (btn == BTN_CLOSE) { w->open=false; if (focused_win==i) focused_win=-1; return; }
        if (btn == BTN_MIN)   { w->minimised=true; focused_win=-1; return; }
        if (btn == BTN_MAX) {
            if (w->maximised) {
                w->x=w->restore_x; w->y=w->restore_y;
                w->w=w->restore_w; w->h=w->restore_h;
                w->maximised=false;
            } else {
                w->restore_x=w->x; w->restore_y=w->y;
                w->restore_w=w->w; w->restore_h=w->h;
                w->x=0; w->y=0; w->w=1024; w->h=768-TASKBAR_H;
                w->maximised=true;
            }
            return;
        }
        /* Drag zone */
        if (btn == -1) {
            w->dragging=true;
            w->drag_ox = mouse.x - w->x;
            w->drag_oy = mouse.y - w->y;
        }
        return;
    }

    /* Click on empty desktop */
    focused_win = -1;
}

/* ════════════════════════════════════════════
 *  Input: Keyboard
 * ════════════════════════════════════════════ */
void gui_key(char c, bool special, uint8_t scancode) {
    (void)special; (void)scancode;
    /* Only route to terminal */
    if (focused_win<0 || windows[focused_win].type!=WIN_TERMINAL) return;

    if (c == '\n' || c == '\r') {
        term_run_cmd();
    } else if (c == '\b') {
        if (term.input_len > 0) {
            term.input_len--;
            term.input[term.input_len] = 0;
        }
    } else if (c >= 32 && c < 127 && term.input_len < TERM_COLS-1) {
        term.input[term.input_len++] = c;
        term.input[term.input_len]   = 0;
    }
}
