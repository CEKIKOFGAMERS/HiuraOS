#include "../include/framebuffer.h"
#include "../include/gui.h"
#include "../include/ps2.h"

extern uint64_t multiboot_info_ptr;

void kernel_main(void) {
    fb_init(multiboot_info_ptr);
    ps2_init();
    gui_init();
    while (1) {
        ps2_poll();
        gui_render();
    }
}