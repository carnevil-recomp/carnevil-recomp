/* Link layout for GAME.EXE v1.0.3: one flat image loaded at 0x800C4000. */
OUTPUT_ARCH(mips)
ENTRY(_start)
SECTIONS
{
    . = 0x800c4000;
    .text : { *(.text) }
    .data : { *(.data) }
    /DISCARD/ : { *(.reginfo) *(.MIPS.abiflags) *(.pdr) *(.mdebug*) *(.comment) *(.gnu.attributes) }
}
