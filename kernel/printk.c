#include <kfs/console.h>
#include <kfs/printk.h>
#include <kfs/serial.h>
#include <kfs/stdio.h>
#include <kfs/string.h>

int console_printk[4] = {
	KFS_LOGLEVEL_DEFAULT, /* console_loglevel */
	KFS_LOGLEVEL_DEFAULT, /* default_message_loglevel */
	KFS_LOGLEVEL_ALERT,	  /* minimum_console_loglevel */
	KFS_LOGLEVEL_DEBUG,	  /* default_console_loglevel */
};

static int printk_last_loglevel = KFS_LOGLEVEL_DEFAULT;
static int printk_last_emitted = 1;

static int clamp_loglevel(int level)
{
	if (level < minimum_console_loglevel)
	{
		return minimum_console_loglevel;
	}
	if (level > KFS_LOGLEVEL_DEBUG)
	{
		return KFS_LOGLEVEL_DEBUG;
	}
	return level;
}

static int vprintk_internal(const char *fmt, va_list ap)
{
	/* TODO(console abstraction):
	 * 今後 terminal_write / serial_write の直接呼び出しを console_drivers[] によるループへ置換し
	 * 出力先を動的登録可能にする (early serial, vga console, future log buffer 等)。
	 */
	const char *msg_fmt = fmt;
	int level = default_message_loglevel;
	int is_cont = 0;

	/* msg_fmtの先頭文字から出力する文字列のログレベルを取得する */
	while (msg_fmt[0] == '\001') /* 001 = SOH (start of heading) . see man ascii */
	{
		char code = msg_fmt[1];
		if (code >= '0' && code <= '7')
		{
			level = code - '0';
			is_cont = 0;
			msg_fmt += 2;
			continue;
		}
		if (code == 'd')
		{
			level = default_message_loglevel;
			is_cont = 0;
			msg_fmt += 2;
			continue;
		}
		if (code == 'c')
		{
			is_cont = 1;
			msg_fmt += 2;
			break;
		}
		break;
	}

	char buffer[1024];
	va_list copy;
	va_copy(copy, ap);
	int len = vsnprintf(buffer, sizeof(buffer), msg_fmt, copy);
	va_end(copy);
	size_t out_len = (size_t)len;
	if (out_len >= sizeof(buffer))
	{
		out_len = sizeof(buffer) - 1;
	}
	int emit = 0;
	if (is_cont)
	{
		level = printk_last_loglevel;
		emit = printk_last_emitted;
	}
	else
	{
		level = clamp_loglevel(level);
		printk_last_loglevel = level;
		emit = (level <= console_loglevel);
	}
	if (out_len > 0 && emit)
	{
		// When the concept of files later emerged, /dev/kmsg would be a natural target for printk output.
		terminal_write(buffer, out_len);
		serial_write(buffer, out_len);
	}
	if (!is_cont)
	{
		printk_last_emitted = emit;
	}
	return len;
}

int printk(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int ret = vprintk_internal(fmt, ap);
	va_end(ap);
	return ret;
}

void kfs_printk_set_console_loglevel(int level)
{
	console_loglevel = clamp_loglevel(level);
}

int kfs_printk_get_console_loglevel(void)
{
	return console_loglevel;
}

int kfs_printk_get_default_loglevel(void)
{
	return default_console_loglevel;
}
