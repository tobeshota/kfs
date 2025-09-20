#ifndef ASM_I386_IO_H
#define ASM_I386_IO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/* Basic port I/O helpers (non-inlined wrappers kept weak in C sources for test overriding). */
	static inline void outb(uint16_t port, uint8_t val)
	{
		__asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
	}
	static inline uint8_t inb(uint16_t port)
	{
		uint8_t r;
		__asm__ volatile("inb %1, %0" : "=a"(r) : "Nd"(port));
		return r;
	}

#ifdef __cplusplus
}
#endif

#endif /* ASM_I386_IO_H */
