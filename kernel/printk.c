#include <linux/printk.h>
#include <linux/string.h>
#include <linux/printk.h>
#include <linux/console.h>
#include <linux/serial.h>

#include <stdarg.h>

int console_printk[4] = {
	KFS_LOGLEVEL_DEFAULT, /* console_loglevel */
	KFS_LOGLEVEL_DEFAULT, /* default_message_loglevel */
	KFS_LOGLEVEL_ALERT,	  /* minimum_console_loglevel */
	KFS_LOGLEVEL_DEBUG,	  /* default_console_loglevel */
};

static int printk_last_loglevel = KFS_LOGLEVEL_DEFAULT;
static int printk_last_emitted = 1;

static void append_char(char **dst, size_t *remaining, size_t *written, char c)
{
	if (*remaining > 1)
	{
		**dst = c;
		(*dst)++;
		(*remaining)--;
	}
	else if (*remaining == 1)
	{
		*remaining = 0;
	}
	(*written)++;
}

static void append_string(char **dst, size_t *remaining, size_t *written, const char *s)
{
	if (!s)
		s = "(null)";
	while (*s)
	{
		append_char(dst, remaining, written, *s);
		s++;
	}
}

static void append_unsigned(char **dst, size_t *remaining, size_t *written, unsigned int value, unsigned int base,
							int uppercase)
{
	char buf[32];
	int idx = 0;
	if (value == 0)
	{
		buf[idx++] = '0';
	}
	else
	{
		while (value > 0)
		{
			unsigned int digit = value % base;
			value /= base;
			if (digit < 10)
				buf[idx++] = (char)('0' + digit);
			else
				buf[idx++] = (char)((uppercase ? 'A' : 'a') + (digit - 10));
		}
	}
	while (idx > 0)
		append_char(dst, remaining, written, buf[--idx]);
}

static void append_signed(char **dst, size_t *remaining, size_t *written, int value)
{
	unsigned int magnitude;
	if (value < 0)
	{
		append_char(dst, remaining, written, '-');
		magnitude = (unsigned int)(-(long long)value);
	}
	else
	{
		magnitude = (unsigned int)value;
	}
	append_unsigned(dst, remaining, written, magnitude, 10, 0);
}

int kfs_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
	char *out = buf;
	size_t remaining = size ? size : 0;
	size_t written = 0;
	if (remaining)
		*out = '\0';
	while (*fmt)
	{
		if (*fmt != '%')
		{
			append_char(&out, &remaining, &written, *fmt++);
			continue;
		}
		fmt++;
		char spec = *fmt ? *fmt++ : '\0';
		switch (spec)
		{
		case '%':
			append_char(&out, &remaining, &written, '%');
			break;
		case 'c': {
			char c = (char)va_arg(ap, int);
			append_char(&out, &remaining, &written, c);
			break;
		}
		case 's':
			append_string(&out, &remaining, &written, va_arg(ap, const char *));
			break;
		case 'd':
			append_signed(&out, &remaining, &written, va_arg(ap, int));
			break;
		case 'u':
			append_unsigned(&out, &remaining, &written, va_arg(ap, unsigned int), 10, 0);
			break;
		case 'x':
			append_unsigned(&out, &remaining, &written, va_arg(ap, unsigned int), 16, 0);
			break;
		case 'X':
			append_unsigned(&out, &remaining, &written, va_arg(ap, unsigned int), 16, 1);
			break;
		default:
			append_char(&out, &remaining, &written, '%');
			if (spec)
				append_char(&out, &remaining, &written, spec);
			break;
		}
	}
	if (size)
	{ /* Ensure NUL termination */
		if (remaining)
			*out = '\0';
		else
			buf[size - 1] = '\0';
	}
	return (int)written;
}

static int clamp_loglevel(int level)
{
	if (level < minimum_console_loglevel)
		return minimum_console_loglevel;
	if (level > KFS_LOGLEVEL_DEBUG)
		return KFS_LOGLEVEL_DEBUG;
	return level;
}

int kfs_snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int len = kfs_vsnprintf(buf, size, fmt, ap);
	va_end(ap);
	return len;
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
	while (msg_fmt[0] == '\001')
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
	char buffer[512];
	va_list copy;
	va_copy(copy, ap);
	int len = kfs_vsnprintf(buffer, sizeof(buffer), msg_fmt, copy);
	va_end(copy);
	size_t out_len = (size_t)len;
	if (out_len >= sizeof(buffer))
		out_len = sizeof(buffer) - 1;
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
		printk_last_emitted = emit;
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
