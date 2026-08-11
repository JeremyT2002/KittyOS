/* linker.ld -- link the kernel at the physical 1 MiB mark.
 *
 * Multiboot loaders drop the kernel image at a fixed physical address and jump
 * to it with paging OFF, so link addresses == physical addresses here.  We use
 * 0x00100000 (1 MiB) because everything below it is reserved by legacy PC
 * conventions (real-mode IVT, BIOS data area, VGA framebuffer, option ROMs).
 *
 * There is deliberately NO higher-half mapping: the page tables built in
 * boot.asm identity-map the first 1 GiB, so a symbol's link address is also its
 * virtual address in long mode.
 */

ENTRY(_start)

SECTIONS
{
    . = 0x00100000;

    /* .multiboot must come first so the header lands in the first 8 KiB of the
     * file, as required by the Multiboot 1 spec. */
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
        *(.text.*)
    }

    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
        *(.data.*)
    }

    /* .bss holds the page tables (4 KiB aligned, see boot.asm) and the stack. */
    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    kernel_end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
    }
}
