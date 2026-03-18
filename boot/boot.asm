bits 32

; =============================================
; Multiboot2 Header (REQUIRED for GRUB2)
; =============================================
section .multiboot2
align 8
mb2_start:
    dd 0xE85250D6               ; Multiboot2 magic
    dd 0                        ; Architecture: i386 protected mode
    dd mb2_end - mb2_start      ; Header length
    dd -(0xE85250D6 + 0 + (mb2_end - mb2_start))  ; Checksum
    ; End tag
    dw 0
    dw 0
    dd 8
mb2_end:

; =============================================
; Stack (16-byte aligned, required by x86_64 ABI)
; =============================================
section .bss
align 16
stack_bottom:
    resb 16384          ; 16 KiB stack
stack_top:

; =============================================
; Entry point
; =============================================
section .text
global _start
extern long_mode_start

_start:
    cli
    mov esp, stack_top
    call long_mode_start

hang:
    hlt
    jmp hang
