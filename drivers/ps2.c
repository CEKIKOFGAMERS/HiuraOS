#include "../include/ps2.h"
#include "../include/gui.h"
#include "../include/types.h"

/* ════════════════════════════════════════════
 *  PS/2 Port I/O
 * ════════════════════════════════════════════ */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

#define PS2_DATA   0x60
#define PS2_STATUS 0x64
#define PS2_CMD    0x64

#define STATUS_OUTPUT_FULL  (1<<0)
#define STATUS_INPUT_FULL   (1<<1)
#define STATUS_MOUSE_DATA   (1<<5)

static void ps2_wait_write(void) {
    int timeout = 100000;
    while ((inb(PS2_STATUS) & STATUS_INPUT_FULL) && --timeout);
}
static void ps2_wait_read(void) {
    int timeout = 100000;
    while (!(inb(PS2_STATUS) & STATUS_OUTPUT_FULL) && --timeout);
}

static void mouse_write(uint8_t cmd) {
    ps2_wait_write(); outb(PS2_CMD,  0xD4);
    ps2_wait_write(); outb(PS2_DATA, cmd);
}
static uint8_t mouse_read(void) {
    ps2_wait_read(); return inb(PS2_DATA);
}

/* ════════════════════════════════════════════
 *  Scancode Set 1 → ASCII  (US layout)
 * ════════════════════════════════════════════ */
static const char sc1_normal[128] = {
    0,0,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,' ',0,
    0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};
static const char sc1_shifted[128] = {
    0,0,'!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',0,
    '*',0,' ',0,
    0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static bool shift_held  = false;
static bool caps_lock   = false;

static void kbd_process(uint8_t sc) {
    bool released = (sc & 0x80) != 0;
    uint8_t code  = sc & 0x7F;

    /* Modifiers */
    if (code == 0x2A || code == 0x36) { shift_held = !released; return; }
    if (code == 0x3A && !released)    { caps_lock  = !caps_lock; return; }
    if (released) return;

    char c = shift_held ? sc1_shifted[code] : sc1_normal[code];
    if (caps_lock && c>='a' && c<='z') c -= 32;
    if (caps_lock && c>='A' && c<='Z' && shift_held) c += 32;

    gui_key(c, false, code);
}

/* ════════════════════════════════════════════
 *  Mouse state machine (3-byte packets)
 * ════════════════════════════════════════════ */
static uint8_t mouse_buf[3];
static int     mouse_phase = 0;

static void mouse_process_packet(void) {
    uint8_t flags = mouse_buf[0];
    int dx =  (int8_t)mouse_buf[1];
    int dy = -(int8_t)mouse_buf[2];   /* Y is inverted in PS/2 */

    bool left  = (flags & 0x01) != 0;
    bool right = (flags & 0x02) != 0;

    /* Overflow check */
    if (flags & 0x40 || flags & 0x80) return;

    gui_mouse_update(dx, dy, left, right);
}

/* ════════════════════════════════════════════
 *  Init
 * ════════════════════════════════════════════ */
void ps2_init(void) {
    /* Disable both PS/2 ports */
    ps2_wait_write(); outb(PS2_CMD, 0xAD);
    ps2_wait_write(); outb(PS2_CMD, 0xA7);

    /* Flush output buffer */
    while (inb(PS2_STATUS) & STATUS_OUTPUT_FULL) inb(PS2_DATA);

    /* Read controller config */
    ps2_wait_write(); outb(PS2_CMD, 0x20);
    ps2_wait_read();
    uint8_t cfg = inb(PS2_DATA);

    /* Enable IRQs for both ports, disable translation */
    cfg |=  0x03;
    cfg &= ~0x40;

    /* Write config back */
    ps2_wait_write(); outb(PS2_CMD, 0x60);
    ps2_wait_write(); outb(PS2_DATA, cfg);

    /* Controller self-test */
    ps2_wait_write(); outb(PS2_CMD, 0xAA);
    ps2_wait_read();
    inb(PS2_DATA); /* 0x55 = ok */

    /* Re-write config (VMware resets it after self-test) */
    ps2_wait_write(); outb(PS2_CMD, 0x60);
    ps2_wait_write(); outb(PS2_DATA, cfg);

    /* Enable port 1 (keyboard) */
    ps2_wait_write(); outb(PS2_CMD, 0xAE);

    /* Enable port 2 (mouse) */
    ps2_wait_write(); outb(PS2_CMD, 0xA8);

    /* Reset keyboard */
    ps2_wait_write(); outb(PS2_DATA, 0xFF);
    ps2_wait_read(); inb(PS2_DATA); /* ACK */
    for (int i=0; i<100000; i++) __asm__("nop");

    /* Reset mouse and enable streaming */
    mouse_write(0xFF);             /* reset */
    mouse_read();                  /* ACK */
    for (int i=0; i<100000; i++) __asm__("nop");
    mouse_write(0xF4);             /* enable streaming */
    mouse_read();                  /* ACK */

    mouse_phase = 0;
}

/* ════════════════════════════════════════════
 *  Poll (called every frame from kernel loop)
 * ════════════════════════════════════════════ */
void ps2_poll(void) {
    int limit = 64;
    while (--limit) {
        uint8_t status = inb(PS2_STATUS);
        if (!(status & STATUS_OUTPUT_FULL)) break;

        uint8_t data = inb(PS2_DATA);

        /* Heuristic: bit 5 of status tells us if this is mouse data.
         * On VMware that bit is unreliable; use packet phase as fallback. */
        bool is_mouse = (status & STATUS_MOUSE_DATA) ||
                        (mouse_phase > 0);

        if (is_mouse) {
            mouse_buf[mouse_phase++] = data;
            if (mouse_phase == 3) {
                mouse_process_packet();
                mouse_phase = 0;
            }
        } else {
            kbd_process(data);
        }
    }
}
