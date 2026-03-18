#include "../include/vga.h"

void kernel_main()
{
    vga_clear();

    // vga_print("x86_64 Kernel Started\n");
    // vga_print("Running in Long Mode\n");
    vga_print("Welcome to HiuraOS!\n");
    vga_print("This kernel still WIP\n");


    while(1)
        __asm__("hlt");
}