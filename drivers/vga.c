#include "../include/vga.h"

volatile unsigned short* VGA = (unsigned short*)0xB8000;
int cursor = 0;

void vga_clear()
{
    for(int i=0;i<80*25;i++)
        VGA[i] = 0x0720;
}

void vga_print(const char* str)
{
    while(*str)
    {
        VGA[cursor++] = 0x0F00 | *str;
        str++;
    }
}