/*
 * Minimal i386 GDT setup header
 */
#ifndef _ASM_I386_DESC_H
#define _ASM_I386_DESC_H

#include <stdint.h>

/* GDT entry indices (like Linux GDT_ENTRY_*) */
#define GDT_ENTRY_NULL 0
#define GDT_ENTRY_KERNEL_CS 1
#define GDT_ENTRY_KERNEL_DS 2
#define GDT_ENTRY_KERNEL_SS 3
#define GDT_ENTRY_USER_CS 4
#define GDT_ENTRY_USER_DS 5
#define GDT_ENTRY_USER_SS 6

#define GDT_ENTRIES 7

/* Selectors (RPL bits appended where needed) */
#define __KERNEL_CS ((GDT_ENTRY_KERNEL_CS) << 3)
#define __KERNEL_DS ((GDT_ENTRY_KERNEL_DS) << 3)
#define __KERNEL_SS ((GDT_ENTRY_KERNEL_SS) << 3)
#define __USER_CS (((GDT_ENTRY_USER_CS) << 3) | 0x3)
#define __USER_DS (((GDT_ENTRY_USER_DS) << 3) | 0x3)
#define __USER_SS (((GDT_ENTRY_USER_SS) << 3) | 0x3)

/* Descriptor pointer structure (lgdt operand) */
struct desc_ptr
{
	uint16_t size;	  /* size - 1 */
	uint32_t address; /* linear address */
} __attribute__((packed));

/* Public init entry */
void gdt_init(void);

#endif /* _ASM_I386_DESC_H */
