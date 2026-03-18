CC = gcc
AS = nasm
LD = ld

CFLAGS = -ffreestanding -m64 -O2 -Wall -Wextra

boot.o:
	$(AS) -f elf64 boot/boot.asm -o boot.o

longmode.o:
	$(AS) -f elf64 boot/longmode.asm -o longmode.o

kernel.o:
	$(CC) $(CFLAGS) -c kernel/kernel.c -o kernel.o

vga.o:
	$(CC) $(CFLAGS) -c drivers/vga.c -o vga.o

kernel.bin: boot.o longmode.o kernel.o vga.o
	$(LD) -T linker.ld -o kernel.bin boot.o longmode.o kernel.o vga.o

iso: kernel.bin
	mkdir -p iso/boot/grub
	cp kernel.bin iso/boot/
	cp grub.cfg iso/boot/grub/
	grub-mkrescue -o myos.iso iso

run: iso
	qemu-system-x86_64 -cdrom myos.iso

clean:
	rm -rf *.o iso kernel.bin myos.iso