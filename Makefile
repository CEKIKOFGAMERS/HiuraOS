CC = gcc
AS = nasm
LD = ld

CFLAGS = -ffreestanding -m64 -O2 -Wall -Wextra
LDFLAGS = -T linker.ld

SRC_C = kernel/kernel.c drivers/vga.c
OBJ_C = $(SRC_C:.c=.o)

boot.o:
	$(AS) -f elf64 boot/boot.asm -o boot.o

kernel/kernel.o: kernel/kernel.c
	$(CC) $(CFLAGS) -c kernel/kernel.c -o kernel/kernel.o

drivers/vga.o: drivers/vga.c
	$(CC) $(CFLAGS) -c drivers/vga.c -o drivers/vga.o

kernel.bin: boot.o kernel/kernel.o drivers/vga.o
	$(LD) $(LDFLAGS) -o kernel.bin boot.o kernel/kernel.o drivers/vga.o

iso: kernel.bin
	mkdir -p iso/boot/grub
	cp kernel.bin iso/boot/
	cp grub.cfg iso/boot/grub/
	grub-mkrescue -o x86os.iso iso

run: iso
	qemu-system-x86_64 -cdrom x86os.iso

clean:
	rm -rf *.o kernel/*.o drivers/*.o iso kernel.bin x86os.iso