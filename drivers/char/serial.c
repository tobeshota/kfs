#include <asm-i386/io.h>
#include <linux/serial.h>
#include <stddef.h>
#include <stdint.h>

#define COM1_PORT 0x3F8

/* I/O indirection layer (weak so tests can override) */
/* weak シンボルはテストで差し替え可能な I/O 間接層。
 * 将来: kfs_io_outb/inb は asm-i386/io.h の outb/inb に薄く委譲する別コンパイルユニットへ抽出可。
 */
__attribute__((weak)) void kfs_io_outb(uint16_t port, uint8_t val)
{
	outb(port, val);
}
/*
kfs_io_inb(PS2_DATA_PORT); は「I/O ポート 0x60 (PS/2 コントローラのデータポート) から 1
バイト読み取る」ための低レベル入力命令ラッパ呼び出しです。
*/
__attribute__((weak)) uint8_t kfs_io_inb(uint16_t port)
{
	return inb(port);
}

static int serial_transmit_empty(void)
{
	return kfs_io_inb(COM1_PORT + 5) & 0x20;
}

void serial_init(void)
{
	kfs_io_outb(COM1_PORT + 1, 0x00);
	kfs_io_outb(COM1_PORT + 3, 0x80);
	kfs_io_outb(COM1_PORT + 0, 0x03);
	kfs_io_outb(COM1_PORT + 1, 0x00);
	kfs_io_outb(COM1_PORT + 3, 0x03);
	kfs_io_outb(COM1_PORT + 2, 0xC7);
	kfs_io_outb(COM1_PORT + 4, 0x0B);
}

void serial_write(const char *data, size_t size)
{
	for (size_t i = 0; i < size; i++)
	{
		char c = data[i];
		if (c == '\n')
		{
			while (!serial_transmit_empty())
			{
			}
			kfs_io_outb(COM1_PORT + 0, (uint8_t)'\r');
		}
		while (!serial_transmit_empty())
		{
		}
		kfs_io_outb(COM1_PORT + 0, (uint8_t)c);
	}
}
