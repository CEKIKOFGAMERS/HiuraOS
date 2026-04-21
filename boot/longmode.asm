bits 32
global long_mode_start
extern kernel_main
extern multiboot_info_ptr

; =============================================
; Page Tables (4096-byte aligned)
; =============================================
section .bss
align 4096
pml4:   resb 4096
pdpt0:  resb 4096
pd0:    resb 4096
pd1:    resb 4096
pd2:    resb 4096
pd3:    resb 4096

; =============================================
; GDT for Long Mode
; =============================================
section .data
align 8
gdt_start:
    dq 0                        ; Null descriptor
    dq 0x00AF9A000000FFFF       ; 64-bit code (ring 0)
    dq 0x00AF92000000FFFF       ; 64-bit data (ring 0)
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dq gdt_start

; =============================================
; Long Mode Setup
; =============================================
section .text

long_mode_start:
    ; --- Zero all page tables ---
    mov edi, pml4
    mov ecx, 6 * 4096 / 4
    xor eax, eax
    rep stosd

    ; --- Wire PML4 -> PDPT -> 4 PDs (covering full 4 GiB) ---
    mov eax, pdpt0
    or  eax, 0b11
    mov [pml4], eax

    mov eax, pd0
    or  eax, 0b11
    mov [pdpt0 + 0*8], eax

    mov eax, pd1
    or  eax, 0b11
    mov [pdpt0 + 1*8], eax

    mov eax, pd2
    or  eax, 0b11
    mov [pdpt0 + 2*8], eax

    mov eax, pd3
    or  eax, 0b11
    mov [pdpt0 + 3*8], eax

    ; --- Identity map 4 GiB via 2 MB pages ---
    ; pd0: 0x0000_0000 - 0x3FFF_FFFF
    mov ecx, 0
.map_pd0:
    mov eax, 0x200000
    mul ecx
    or  eax, 0b10000011
    mov [pd0 + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .map_pd0

    ; pd1: 0x4000_0000 - 0x7FFF_FFFF
    mov ecx, 0
.map_pd1:
    mov eax, 0x200000
    mul ecx
    add eax, 0x40000000
    or  eax, 0b10000011
    mov [pd1 + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .map_pd1

    ; pd2: 0x8000_0000 - 0xBFFF_FFFF
    mov ecx, 0
.map_pd2:
    mov eax, 0x200000
    mul ecx
    add eax, 0x80000000
    or  eax, 0b10000011
    mov [pd2 + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .map_pd2

    ; pd3: 0xC000_0000 - 0xFFFF_FFFF (covers MMIO / VRAM)
    mov ecx, 0
.map_pd3:
    mov eax, 0x200000
    mul ecx
    add eax, 0xC0000000
    or  eax, 0b10000011
    mov [pd3 + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .map_pd3

    ; --- Load CR3 ---
    mov eax, pml4
    mov cr3, eax

    ; --- Enable PAE ---
    mov eax, cr4
    or  eax, 1 << 5
    mov cr4, eax

    ; --- Enable Long Mode (EFER.LME) ---
    mov ecx, 0xC0000080
    rdmsr
    or  eax, 1 << 8
    wrmsr

    ; --- Enable Paging ---
    mov eax, cr0
    or  eax, 1 << 31
    mov cr0, eax

    ; --- Load GDT & far-jump to 64-bit ---
    lgdt [gdt_descriptor]
    jmp  0x08:long_mode_entry

; =============================================
; 64-bit Entry
; =============================================
bits 64

long_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Sign-extend multiboot_info_ptr to full 64-bit
    mov eax, dword [multiboot_info_ptr]
    mov qword [multiboot_info_ptr], rax

    ; Remap PIC: IRQ0-7 -> INT 0x20, IRQ8-15 -> INT 0x28
    mov al, 0x11
    out 0x20, al
    out 0xA0, al
    mov al, 0x20
    out 0x21, al
    mov al, 0x28
    out 0xA1, al
    mov al, 0x04
    out 0x21, al
    mov al, 0x02
    out 0xA1, al
    mov al, 0x01
    out 0x21, al
    out 0xA1, al
    ; Mask all IRQs (we poll)
    mov al, 0xFF
    out 0x21, al
    out 0xA1, al

    ; GRUB zeroes .bss for Multiboot2 -- no need to do it manually

    call kernel_main

halt:
    hlt
    jmp halt