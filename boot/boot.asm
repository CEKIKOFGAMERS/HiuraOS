bits 64

section .multiboot
align 8
dd 0xE85250D6
dd 0
dd header_end - header_start
dd -(0xE85250D6 + 0 + (header_end - header_start))

header_start:
dw 0
dw 0
dd 8
header_end:

section .text
global _start
extern kernel_main

_start:
    cli
    call kernel_main

hang:
    hlt
    jmp hang