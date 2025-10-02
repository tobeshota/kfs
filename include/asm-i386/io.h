#ifndef ASM_I386_IO_H
#define ASM_I386_IO_H

#include <stdint.h>

/** ポートマップドI/O **/
/* I/Oポート番号port指定のデバイスに対してvalを出力する */
static inline void outb(uint16_t port, uint8_t val)
{
	__asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
/* I/Oポート番号port指定のデバイスに対してvalを入力する */
static inline uint8_t inb(uint16_t port)
{
	uint8_t r;
	__asm__ volatile("inb %1, %0" : "=a"(r) : "Nd"(port));
	return r;
}

#endif /* ASM_I386_IO_H */
