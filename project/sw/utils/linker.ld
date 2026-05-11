ENTRY(_start)

MEMORY
{
    IMEM   (rx) : ORIGIN = 0x10000000, LENGTH = 128K
    DMEM   (rw) : ORIGIN = 0x20000000, LENGTH = 64K
    LM (rx) : ORIGIN = 0, LENGTH = LENGTH(DMEM) + LENGTH(IMEM) /* load memory for the binary image only */
}


SECTIONS
{

    .boot_table 0x0 :
    {
        _boot_table = .;

        LONG(ADDR(.text)); /* VMA: where the section lives under execution, the actual address */
        LONG(LOADADDR(.text)); /* LMA: The load memory address, where the section lives in the binary image. Should be 0x24 for .text */
        LONG(SIZEOF(.text)); /* lenght of section */

        LONG(ADDR(.rodata));
        LONG(LOADADDR(.rodata));
        LONG(SIZEOF(.rodata));

        LONG(ADDR(.data));
        LONG(LOADADDR(.data));
        LONG(SIZEOF(.data));
    } > LM


    .text :
    {
        _stext = .;
        KEEP(*(.text.start))  /* _start symbol */
        *(.text*)             /* code */
        _etext = .;
    } > IMEM AT > LM

    .rodata :
    {
        _srodata = .;
        *(.rodata*)           /* constants, string literals */
        _erodata = .;
    } > DMEM AT > LM

    .data : 
    {
        _sdata = .;
        *(.data*)             /* initialized globals */
        _edata = .;
    } > DMEM AT > LM

    .bss :
    {
        _sbss = .;           /* zero initialized variables. Does not take up space in binary, just used for crt0 clearing */
        *(.bss*)
        *(COMMON)
        _ebss = .;
    } > DMEM

    PROVIDE(_stack_top = ORIGIN(DMEM) + LENGTH(DMEM));
}