#include <stdint.h>
#include "../include/vga.h"

void kernel_main()
{
    vga_clear();
    vga_print("Welcome to My x86_64 Kernel\n");
    vga_print("Kernel started successfully.\n");

    while(1)
    {
        __asm__("hlt");
    }
}