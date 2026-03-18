ENTRY(_start)

SECTIONS
{
    . = 1M;                     /* Load kernel at 1 MiB (standard for Multiboot) */

    /* Multiboot2 header MUST be within first 32KB of the file */
    .multiboot2 :
    {
        KEEP(*(.multiboot2))    /* KEEP prevents linker from discarding it */
    }

    /* Code */
    .text ALIGN(4096) :
    {
        *(.text)
    }

    /* Read-only data */
    .rodata ALIGN(4096) :
    {
        *(.rodata*)
    }

    /* Initialized data */
    .data ALIGN(4096) :
    {
        *(.data)
    }

    /* Uninitialized data (zeroed by boot) */
    .bss ALIGN(4096) :
    {
        *(COMMON)
        *(.bss)
    }
}
