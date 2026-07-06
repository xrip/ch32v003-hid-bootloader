ENTRY(InterruptVector)

MEMORY
{
    BOOT (rx) : ORIGIN = 0x00000000, LENGTH = 1920
    RAM (xrw) : ORIGIN = 0x20000000, LENGTH = 2K
}

SECTIONS
{
    .init :
    {
        KEEP(*(.init))
    } >BOOT

    .text :
    {
        *(.text.vector_handler)
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata*)
        *(.srodata .srodata.*)
        . = ALIGN(4);
    } >BOOT

    .data :
    {
        PROVIDE(_data_vma = .);
        *(.data .data.*)
        *(.sdata .sdata.*)
        . = ALIGN(4);
        PROVIDE(_edata = .);
    } >RAM AT>BOOT

    .bss :
    {
        PROVIDE(_sbss = .);
        *(.sbss*)
        *(.bss*)
        *(COMMON*)
        . = ALIGN(4);
        PROVIDE(_ebss = .);
    } >RAM

    PROVIDE(_end = _ebss);
    PROVIDE(_eusrstack = ORIGIN(RAM) + LENGTH(RAM));
}
