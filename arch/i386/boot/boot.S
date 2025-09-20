/* Declare constants for the multiboot header. */
.set ALIGN,    1<<0             /* align loaded modules on page boundaries */
.set MEMINFO,  1<<1             /* provide memory map */
.set FLAGS,    ALIGN | MEMINFO  /* this is the Multiboot 'flag' field */
.set MAGIC,    0x1BADB002       /* 'magic number' lets bootloader find the header */
.set CHECKSUM, -(MAGIC + FLAGS) /* checksum of above, to prove we are multiboot */

/* Multiboot header must be within first 8 KiB and 4-byte aligned */
.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

/* 16 KiB stack, 16-byte aligned */
.section .bss
.align 16
stack_bottom:
.skip 16384
stack_top:

.section .text
.global _start
.type _start, @function
_start:
    /* Set up stack */
    mov $stack_top, %esp

    /* Enter high-level kernel */
    call start_kernel

    /* If returns, halt forever */
    cli
1:  hlt
    jmp 1b

.size _start, . - _start

