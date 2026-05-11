.section .text.start
.global _start

_start:
    la sp, _stack_top

clear_bss:
    la t0, _sbss
    la t1, _ebss

zero_bss:
    bge t0, t1, call_main
    sw  zero, 0(t0)
    addi t0, t0, 4
    j zero_bss

call_main:
    call main

hang:
    j hang
