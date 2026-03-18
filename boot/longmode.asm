bits 32
global long_mode_start
extern kernel_main

; =============================================
; Page Tables (must be 4096-byte aligned)
; =============================================
section .bss
align 4096
pml4:   resb 4096       ; Page Map Level 4
pdpt:   resb 4096       ; Page Directory Pointer Table
pd:     resb 4096       ; Page Directory (holds 2MB entries)

; =============================================
; GDT for Long Mode
; =============================================
section .data
align 8
gdt_start:
    dq 0                        ; Null descriptor (required)
    dq 0x00AF9A000000FFFF       ; 64-bit code segment (ring 0)
    dq 0x00AF92000000FFFF       ; 64-bit data segment (ring 0)
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; Limit (size - 1)
    dq gdt_start                ; Base address (64-bit pointer — use dq, not dd!)

; =============================================
; Long Mode Setup
; =============================================
section .text

long_mode_start:

    ; --- 1. Zero out all page tables ---
    mov edi, pml4
    mov ecx, 3 * 4096 / 4      ; 3 tables × 4096 bytes / 4 bytes per dword
    xor eax, eax
    rep stosd

    ; --- 2. Wire up PML4 → PDPT → PD ---
    ; PML4[0] = &pdpt | Present | Writable
    mov eax, pdpt
    or  eax, 0b11
    mov [pml4], eax

    ; PDPT[0] = &pd | Present | Writable
    mov eax, pd
    or  eax, 0b11
    mov [pdpt], eax

    ; --- 3. Identity-map first 1 GiB using 2MB pages ---
    ; PD[0..511] = (i * 0x200000) | Present | Writable | 2MB (PS bit)
    mov ecx, 0                  ; Loop counter (page index)
.map_pd:
    mov eax, 0x200000           ; 2 MiB
    mul ecx                     ; eax = index * 2MB
    or  eax, 0b10000011         ; Present | Writable | PageSize (2MB)
    mov [pd + ecx * 8], eax     ; Store low 32 bits
    ; High 32 bits are zero (physical addresses < 4GB)
    inc ecx
    cmp ecx, 512
    jne .map_pd

    ; --- 4. Load CR3 with PML4 physical address ---
    mov eax, pml4
    mov cr3, eax

    ; --- 5. Enable PAE (Physical Address Extension) ---
    mov eax, cr4
    or  eax, 1 << 5             ; PAE bit
    mov cr4, eax

    ; --- 6. Enable Long Mode in EFER MSR ---
    mov ecx, 0xC0000080         ; EFER MSR number
    rdmsr
    or  eax, 1 << 8             ; LME (Long Mode Enable) bit
    wrmsr

    ; --- 7. Enable Paging (also activates Long Mode) ---
    mov eax, cr0
    or  eax, 1 << 31            ; PG (Paging) bit
    mov cr0, eax

    ; --- 8. Load GDT and far-jump to flush pipeline ---
    lgdt [gdt_descriptor]
    jmp  0x08:long_mode_entry   ; 0x08 = 64-bit code segment selector

; =============================================
; 64-bit Long Mode Entry
; =============================================
bits 64

long_mode_entry:
    ; Load data segment into all segment registers
    mov ax, 0x10                ; 0x10 = 64-bit data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Call the C kernel
    call kernel_main

halt:
    hlt
    jmp halt
