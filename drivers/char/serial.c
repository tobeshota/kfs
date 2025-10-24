#include <asm-i386/io.h>
#include <kfs/serial.h>
#include <stddef.h>
#include <stdint.h>

/* Communication Port。
 * カーネルのデバッグ用途として、現在communication portへの出力をQEMUがstdioにリダイレクトしている。
 */
#define COM1_PORT 0x3F8
#define COM2_PORT 0x2F8 /* 入力用として2つ目のシリアルポートを使用 */

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

/* シリアルポートの送信バッファが空かどうかを調べる */
static int serial_transmit_empty(void)
{
	/* 0x20 := Read "byte 0" from internal RAM.
	 * @see https://wiki.osdev.org/I8042_PS/2_Controller
	 */
	return kfs_io_inb(COM1_PORT + 5) & 0x20;
}

/* シリアルポートの受信バッファにデータがあるかどうかを調べる */
static int serial_received(void)
{
	/* 0x01 := Data ready bit
	 * @see https://wiki.osdev.org/Serial_Ports
	 * COM2から入力を読み取る
	 */
	return kfs_io_inb(COM2_PORT + 5) & 0x01;
}

/* シリアルポートから1バイト読み取る（ノンブロッキング）
 * @return 読み取った文字。データがない場合は-1
 */
int serial_read(void)
{
	if (!serial_received())
		return -1;
	return (int)kfs_io_inb(COM2_PORT);
}

/* シリアル通信の初期化 */
void serial_init(void)
{
	/* COM1の初期化（出力用） */
	kfs_io_outb(COM1_PORT + 1, 0x00); /*割り込みを無効化*/
	kfs_io_outb(COM1_PORT + 3, 0x80); /*ボーレート設定モード開始*/
	kfs_io_outb(COM1_PORT + 0, 0x03); /*ボーレートを38400bpsにする (16ビットの下位ビット)*/
	kfs_io_outb(COM1_PORT + 1, 0x00); /*ボーレートを38400bpsにする (16ビットの上位ビット)*/
	kfs_io_outb(COM1_PORT + 3, 0x03); /*ボーレート設定モード終了する。また、シリアル接続のビットを8N1に設定する*/
	kfs_io_outb(COM1_PORT + 2, 0xC7); /*FIFOを有効化, 受信用14バイトのバッファを設ける*/
	kfs_io_outb(COM1_PORT + 4, 0x0B); /*RTS/DSRを有効化*/

	/* COM2の初期化（入力用） */
	kfs_io_outb(COM2_PORT + 1, 0x00); /*割り込みを無効化*/
	kfs_io_outb(COM2_PORT + 3, 0x80); /*ボーレート設定モード開始*/
	kfs_io_outb(COM2_PORT + 0, 0x03); /*ボーレートを38400bpsにする (16ビットの下位ビット)*/
	kfs_io_outb(COM2_PORT + 1, 0x00); /*ボーレートを38400bpsにする (16ビットの上位ビット)*/
	kfs_io_outb(COM2_PORT + 3, 0x03); /*ボーレート設定モード終了する。また、シリアル接続のビットを8N1に設定する*/
	kfs_io_outb(COM2_PORT + 2, 0xC7); /*FIFOを有効化, 受信用14バイトのバッファを設ける*/
	kfs_io_outb(COM2_PORT + 4, 0x0B); /*RTS/DSRを有効化*/
}

/* シリアルポートに文字列を書き込む(PMIO) */
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
