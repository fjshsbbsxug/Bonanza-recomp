OUTPUT_ARCH(m68k)
ENTRY(_start)
SECTIONS
{
    . = 0;
    .text : { *(.text) }
    /DISCARD/ : { *(.data) *(.bss) *(.comment) *(.note*) }
}
