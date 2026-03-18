# HiuraOS — x86_64 Bare-Metal Kernel

A hobby operating system kernel written from scratch in NASM assembly and C, targeting the x86_64 architecture. It boots via GRUB2/Multiboot2, transitions from 32-bit protected mode into 64-bit long mode, and runs a graphical desktop environment rendered directly into a VESA/VBE linear framebuffer with no external libraries whatsoever.

---

## Features

- **Multiboot2 boot** via GRUB2 with VBE framebuffer request tag
- **32-bit → 64-bit long mode transition** with full 4-level paging (identity maps all 4 GiB)
- **VESA/VBE framebuffer** at 1024×768×32bpp, parsed from Multiboot2 tags
- **Double-buffered rendering** — all drawing goes to a RAM backbuffer, flushed to VRAM once per frame, eliminating flicker
- **GUI desktop** with:
  - Navy dot-grid wallpaper
  - Desktop icons (Terminal, About, Files) with hover highlights
  - Draggable, resizable windows with traffic-light close/minimise/maximise buttons
  - Taskbar with window buttons, start button, and clock
  - Hardware PS/2 mouse cursor with per-element hover colouring
- **Interactive terminal** with scrollback history, blinking cursor, and built-in commands
- **PS/2 keyboard** with full scancode set 1 decoding, Shift and Caps Lock support
- **PS/2 mouse** with polled packet decoding compatible with VirtualBox and VMware
- **8259 PIC remapping** to avoid IRQ/exception vector collisions
- **No libc, no stdlib, no external dependencies** — zero imports

---

## Project Structure

```
x86_64-kernel/
│
├── boot/
│   ├── boot.asm          # Multiboot2 header, stack, entry point (_start)
│   └── longmode.asm      # 32→64-bit transition, paging, GDT, IDT, .bss zero
│
├── kernel/
│   └── kernel.c          # kernel_main(): init sequence + main poll loop
│
├── drivers/
│   ├── framebuffer.c     # Multiboot2 tag parser, backbuffer, draw primitives
│   ├── gui.c             # Desktop, windows, taskbar, mouse, terminal, keyboard
│   └── ps2.c             # PS/2 controller init, mouse packets, keyboard scancodes
│
├── include/
│   ├── types.h           # Freestanding integer types (uint8_t … uint64_t, bool)
│   ├── framebuffer.h     # Framebuffer struct + drawing API
│   ├── gui.h             # Window/Mouse types, colour constants, GUI API
│   └── ps2.h             # PS/2 init + poll API
│
├── linker.ld             # Places .multiboot2 first, kernel at 1 MiB
├── grub.cfg              # GRUB2 boot entry using multiboot2 command
└── Makefile              # Build, ISO creation, QEMU launch
```

---

## How It Boots

```
GRUB2 reads kernel.iso
  └─ finds Multiboot2 header in kernel.bin (within first 32 KB)
       └─ loads kernel at 1 MiB, passes info pointer in EBX
            └─ _start  (boot.asm, 32-bit)
                 ├─ saves EBX → multiboot_info_ptr
                 ├─ sets ESP = stack_top
                 └─ calls long_mode_start  (longmode.asm, 32-bit)
                      ├─ installs our GDT (flushes GRUB's GDT)
                      ├─ zeros 6 page tables (pml4, pdpt, pd0–pd3)
                      ├─ identity maps 4 GiB with 2 MiB pages
                      ├─ loads CR3, enables PAE, sets EFER.LME, enables PG
                      ├─ far jumps to 64-bit code segment → long mode active
                      ├─ reloads RSP (full 64-bit, 16-byte aligned)
                      ├─ builds 256-entry IDT (all vectors → isr_stub)
                      ├─ remaps 8259 PIC to INT 0x20–0x2F
                      ├─ zeros .bss (__bss_start → __bss_end)
                      └─ calls kernel_main()  (kernel.c, 64-bit C)
                           ├─ fb_init()   — parse Multiboot2 framebuffer tag
                           ├─ ps2_init()  — init PS/2 controller + mouse
                           ├─ gui_init()  — zero all state, open welcome windows
                           └─ loop: ps2_poll() → gui_render()
```

---

## Building

### Prerequisites

On Ubuntu/Debian:

```bash
sudo apt install build-essential nasm grub-pc-bin grub-common xorriso mtools
```

On Arch Linux:

```bash
sudo pacman -S base-devel nasm grub xorriso mtools
```

### Build commands

```bash
# Build kernel.bin only
make

# Build bootable kernel.iso (required for VM)
make iso

# Build ISO and run in QEMU
make run

# Remove all build artefacts
make clean
```

The build produces `kernel.iso` in the project root. Boot this file in VirtualBox, VMware, or QEMU.

---

## Running in a VM

### VirtualBox

1. Create a new VM — **Type:** Other, **Version:** Other/Unknown (64-bit)
2. RAM: 128 MB or more
3. Storage: attach `kernel.iso` as an optical drive
4. Display: set to **VMSVGA** or **VBoxVGA**
5. Boot the VM

### VMware Workstation / Player

1. Create a new VM — **Guest OS:** Other 64-bit
2. RAM: 128 MB or more
3. Add a CD/DVD drive pointing to `kernel.iso`
4. Boot the VM

### QEMU

```bash
qemu-system-x86_64 -cdrom kernel.iso -m 128M -vga std
```

---

## Using the Terminal

Click the **Terminal** icon on the desktop (or the taskbar button) to focus it, then type. The terminal only receives keyboard input when it is the **active focused window** — click its title bar first if another window is in front.

| Command | Description |
|---|---|
| `help` | List all available commands |
| `clear` | Clear the terminal history |
| `uname` | Show kernel version and build info |
| `about` | Same as `uname` |
| `ls` | List kernel source files |
| `echo <text>` | Print text back to the terminal |

**Keyboard shortcuts:**
- **Enter** — submit command
- **Backspace** — delete last character
- **Shift** — uppercase / shifted symbols
- **Caps Lock** — toggle caps

---

## GUI Interactions

| Action | Result |
|---|---|
| Click desktop icon | Opens corresponding window |
| Click window title bar | Focuses window |
| Drag window title bar | Moves the window |
| Click red button (×) | Closes the window |
| Click yellow button (−) | Minimises to taskbar |
| Click green button (□) | Toggles maximise / restore |
| Click taskbar window button | Focus or restore window; click active window to minimise |
| Hover any button | Visual highlight |

---

## Architecture Notes

### No paging complexity beyond identity map
All physical memory is identity-mapped using 2 MiB pages across four page directories, covering the full 32-bit address space (0x00000000–0xFFFFFFFF). This includes MMIO regions above 3 GiB where the VBE framebuffer typically lives.

### Double buffering
`framebuffer.c` maintains a 3 MB static array (`backbuf[1024 * 768]`) in `.bss`. Every draw call writes here. `fb_present()` is called once at the end of each frame and bulk-copies the backbuffer to VRAM using 64-bit wide stores (2 pixels per write), which prevents any mid-draw frame from ever reaching the monitor.

### PS/2 compatibility
The PS/2 init sequence is written to work correctly on both VirtualBox and VMware. Key differences handled: VMware's `0xAA` self-test resets the config register (so config is written after self-test); VMware doesn't respond to the `0xA9` mouse port test (so it's skipped); the `MOUSE_DATA` status bit is unreliable on VMware in polled mode (so a bit-3 heuristic is used as fallback).

### Interrupt handling
All 256 IDT vectors point to a single `isr_stub` that sends EOI to both PICs and executes `iretq`. Input is polled rather than interrupt-driven, so the CPU never halts in the main loop — `ps2_poll()` reads the PS/2 controller buffer directly on every frame.

---

## File Reference

| File | Responsibility |
|---|---|
| `boot/boot.asm` | Multiboot2 header (magic, framebuffer tag, end tag), stack, `_start` entry point, saves Multiboot2 info pointer |
| `boot/longmode.asm` | GDT reload, 4-level page table setup (4 GiB identity map), PAE/EFER/PG bits, 256-entry IDT, PIC remap, `.bss` zero, `kernel_main` call |
| `kernel/kernel.c` | Top-level init sequence and spin loop |
| `drivers/framebuffer.c` | Multiboot2 tag walker, backbuffer, `fb_fill_rect` (optimised row-stamp), `fb_draw_char` (8×16 bitmap font), `fb_present` |
| `drivers/gui.c` | Desktop, icon rendering, window chrome, taskbar, mouse cursor, hover detection, drag/resize, terminal ring buffer, command processor |
| `drivers/ps2.c` | PS/2 controller init (disable/flush/test/reset/enable), 3-byte mouse packet state machine, scancode set 1 decoder with Shift/Caps |
| `include/types.h` | Freestanding integer types, `bool`, `NULL` |
| `include/framebuffer.h` | `Framebuffer` struct, draw API declarations |
| `include/gui.h` | `Window`, `Mouse`, `DeskIcon` types, colour constants, GUI API |
| `include/ps2.h` | `ps2_init`, `ps2_poll` declarations |
| `linker.ld` | Section layout: `.multiboot2` first, kernel at 1 MiB, exports `__bss_start`/`__bss_end` |
| `grub.cfg` | Single `multiboot2` menu entry, zero timeout |
| `Makefile` | NASM + GCC build rules, ISO creation via `grub-mkrescue`, QEMU launch target |

---

## Known Limitations

- No memory allocator — all data structures are statically sized
- No real-time clock driver — the taskbar clock is static (`00:00:00`)
- No filesystem — `ls` returns a hardcoded list
- Mouse input is polled, not interrupt-driven
- No SMP support — only CPU0 is used
- No ACPI shutdown/reboot

---

## License

This project is released into the public domain. Do whatever you want with it.
