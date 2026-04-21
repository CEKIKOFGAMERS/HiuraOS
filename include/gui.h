#ifndef GUI_H
#define GUI_H

#include "types.h"
#include "framebuffer.h"

/* ════════════════════════════════════════════
 *  XP-Style Colour Palette
 * ════════════════════════════════════════════ */
#define COL_DESKTOP_TOP    0x3A6EA5   /* Luna blue-teal top */
#define COL_DESKTOP_BOT    0x1E4178   /* Luna blue darker */

#define COL_TITLEBAR_TOP   0x0A246A   /* XP dark title gradient top */
#define COL_TITLEBAR_MID   0x3A6EA5   /* XP title gradient mid */
#define COL_TITLEBAR_BOT   0x245EDC   /* XP title gradient bottom */
#define COL_TITLEBAR_INACT 0x7191BC   /* Inactive title */

#define COL_WINDOW_BG      0xFFFFFF
#define COL_WINDOW_BORDER  0x0054E3
#define COL_WINDOW_SHADOW  0x000000

#define COL_TASKBAR_TOP    0x245EDC   /* XP taskbar gradient */
#define COL_TASKBAR_BOT    0x1C3F9C

#define COL_START_BTN_TOP  0x5AC85A   /* XP green start button */
#define COL_START_BTN_BOT  0x2D6B2D

#define COL_BTN_FACE       0xECE9D8   /* XP button face */
#define COL_BTN_HILIGHT    0xFFFFFF
#define COL_BTN_SHADOW     0xACA899
#define COL_BTN_DKSHADOW   0x716F64

#define COL_WHITE          0xFFFFFF
#define COL_BLACK          0x000000
#define COL_YELLOW         0xFFFF00
#define COL_SELECTION      0x316AC5

#define TITLEBAR_H         26
#define TASKBAR_H          34
#define BORDER_W           3
#define ICON_W             48
#define ICON_H             48
#define MAX_WINDOWS        8
#define MAX_ICONS          6
#define TERM_COLS          60
#define TERM_ROWS          20
#define TERM_HISTORY       200

/* ════════════════════════════════════════════
 *  Data Types
 * ════════════════════════════════════════════ */

typedef enum {
    WIN_NORMAL,
    WIN_TERMINAL,
    WIN_ABOUT,
    WIN_FILES
} WinType;

typedef struct {
    bool     open;
    bool     minimised;
    bool     maximised;
    bool     focused;
    WinType  type;
    int      x, y, w, h;
    int      restore_x, restore_y, restore_w, restore_h;
    char     title[64];
    /* dragging state */
    bool     dragging;
    int      drag_ox, drag_oy;
} Window;

typedef struct {
    int      x, y;
    bool     left, right;
    bool     left_prev;
} Mouse;

typedef struct {
    int      x, y;
    uint32_t icon_color;
    char     label[32];
    WinType  target;
} DeskIcon;

/* Terminal state */
typedef struct {
    char     lines[TERM_HISTORY][TERM_COLS + 1];
    int      top;       /* oldest visible line */
    int      count;     /* total lines stored  */
    char     input[TERM_COLS + 1];
    int      input_len;
    int      blink;     /* blink counter */
    bool     cursor_on;
} Terminal;

/* ════════════════════════════════════════════
 *  GUI API
 * ════════════════════════════════════════════ */
void gui_init(void);
void gui_render(void);
void gui_mouse_update(int dx, int dy, bool left, bool right);
void gui_key(char c, bool special, uint8_t scancode);

#endif
