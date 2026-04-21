CC  = gcc
AS  = nasm
LD  = ld

CFLAGS  = -ffreestanding -m64 -O2 -Wall -Wextra \
          -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
          -fno-stack-protector -fno-pie

LDFLAGS = -T linker.ld -nostdlib

OBJS = boot.o longmode.o kernel.o framebuffer.o gui.o ps2.o

# ── Object rules ───────────────────────────────────────
boot.o:
	$(AS) -f elf64 boot/boot.asm -o boot.o

longmode.o:
	$(AS) -f elf64 boot/longmode.asm -o longmode.o

kernel.o:
	$(CC) $(CFLAGS) -c kernel/kernel.c -o kernel.o

framebuffer.o:
	$(CC) $(CFLAGS) -c drivers/framebuffer.c -o framebuffer.o

gui.o:
	$(CC) $(CFLAGS) -c drivers/gui.c -o gui.o

ps2.o:
	$(CC) $(CFLAGS) -c drivers/ps2.c -o ps2.o

# ── Link ───────────────────────────────────────────────
kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) -o kernel.bin $(OBJS)

# ── ISO ────────────────────────────────────────────────
iso: kernel.bin
	mkdir -p iso/boot/grub
	cp kernel.bin iso/boot/
	cp grub.cfg   iso/boot/grub/
	grub-mkrescue -o myos.iso iso

# ── Run ────────────────────────────────────────────────
run: iso
	qemu-system-x86_64 -cdrom myos.iso -m 128M -vga std

# ── Clean ──────────────────────────────────────────────
clean:
	rm -rf *.o *.bin *.iso iso/boot/kernel.bin
