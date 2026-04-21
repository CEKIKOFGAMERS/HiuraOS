bits 32

; =============================================
; Multiboot2 Header with VBE Framebuffer Tag
; =============================================
section .multiboot2
align 8
mb2_start:
    dd 0xE85250D6               ; Multiboot2 magic
    dd 0                        ; Architecture: i386 protected mode
    dd mb2_end - mb2_start      ; Header length
    dd -(0xE85250D6 + 0 + (mb2_end - mb2_start))  ; Checksum

    ; Framebuffer request tag (type 5) — 1024x768x32
    align 8
    dw 5
    dw 0
    dd 20
    dd 1024
    dd 768
    dd 32

    ; End tag
    align 8
    dw 0
    dw 0
    dd 8
mb2_end:

; =============================================
; Stack (16-byte aligned)
; =============================================
section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

; =============================================
; Save Multiboot2 info pointer (exported to C)
; =============================================
section .data
global multiboot_info_ptr
multiboot_info_ptr: dq 0

; =============================================
; Entry point
; =============================================
section .text
global _start
extern long_mode_start

_start:
    cli
    mov [multiboot_info_ptr], ebx   ; EBX = Multiboot2 info ptr from GRUB
    mov esp, stack_top
    call long_mode_start

hang:
    hlt
    jmp hang
