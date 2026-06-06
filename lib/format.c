#include <kfs/stdio.h>

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
	{
		s = "(null)";
	}
	while (*s)
	{
		append_char(dst, remaining, written, *s);
		s++;
	}
}

static void append_unsigned_with_width(char **dst, size_t *remaining, size_t *written, unsigned int value,
									   unsigned int base, int uppercase, int width, char pad_char)
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
			{
				buf[idx++] = (char)('0' + digit);
			}
			else
			{
				buf[idx++] = (char)((uppercase ? 'A' : 'a') + (digit - 10));
			}
		}
	}
	while (idx < width)
	{
		append_char(dst, remaining, written, pad_char);
		width--;
	}
	while (idx > 0)
	{
		append_char(dst, remaining, written, buf[--idx]);
	}
}

static void append_unsigned(char **dst, size_t *remaining, size_t *written, unsigned int value, unsigned int base,
							int uppercase)
{
	append_unsigned_with_width(dst, remaining, written, value, base, uppercase, 0, ' ');
}

static void append_unsigned_long_with_width(char **dst, size_t *remaining, size_t *written, unsigned long value,
											unsigned int base, int uppercase, int width, char pad_char)
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
			unsigned long digit = value % base;
			value /= base;
			if (digit < 10)
			{
				buf[idx++] = (char)('0' + digit);
			}
			else
			{
				buf[idx++] = (char)((uppercase ? 'A' : 'a') + (digit - 10));
			}
		}
	}
	while (idx < width)
	{
		append_char(dst, remaining, written, pad_char);
		width--;
	}
	while (idx > 0)
	{
		append_char(dst, remaining, written, buf[--idx]);
	}
}

static void append_unsigned_long(char **dst, size_t *remaining, size_t *written, unsigned long value, unsigned int base,
								 int uppercase)
{
	append_unsigned_long_with_width(dst, remaining, written, value, base, uppercase, 0, ' ');
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

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
	char *out = buf;
	size_t remaining = size ? size : 0;
	size_t written = 0;
	if (remaining)
	{
		*out = '\0';
	}
	while (*fmt)
	{
		if (*fmt != '%')
		{
			append_char(&out, &remaining, &written, *fmt++);
			continue;
		}
		fmt++;

		char pad_char = ' ';
		if (*fmt == '0')
		{
			pad_char = '0';
			fmt++;
		}

		int width = 0;
		while (*fmt >= '0' && *fmt <= '9')
		{
			width = width * 10 + (*fmt - '0');
			fmt++;
		}

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
			append_unsigned_with_width(&out, &remaining, &written, va_arg(ap, unsigned int), 10, 0, width, pad_char);
			break;
		case 'x':
			append_unsigned_with_width(&out, &remaining, &written, va_arg(ap, unsigned int), 16, 0, width, pad_char);
			break;
		case 'X':
			append_unsigned_with_width(&out, &remaining, &written, va_arg(ap, unsigned int), 16, 1, width, pad_char);
			break;
		case 'l': {
			if (*fmt == 'u')
			{
				fmt++;
				append_unsigned_long_with_width(&out, &remaining, &written, va_arg(ap, unsigned long), 10, 0, width,
												pad_char);
			}
			else if (*fmt == 'x')
			{
				fmt++;
				append_unsigned_long_with_width(&out, &remaining, &written, va_arg(ap, unsigned long), 16, 0, width,
												pad_char);
			}
			else if (*fmt == 'X')
			{
				fmt++;
				append_unsigned_long_with_width(&out, &remaining, &written, va_arg(ap, unsigned long), 16, 1, width,
												pad_char);
			}
			else if (*fmt == 'd')
			{
				fmt++;
				long value = va_arg(ap, long);
				if (value < 0)
				{
					append_char(&out, &remaining, &written, '-');
					append_unsigned_long(&out, &remaining, &written, (unsigned long)(-value), 10, 0);
				}
				else
				{
					append_unsigned_long(&out, &remaining, &written, (unsigned long)value, 10, 0);
				}
			}
			else
			{
				append_char(&out, &remaining, &written, '%');
				append_char(&out, &remaining, &written, 'l');
			}
			break;
		}
		case 'p': {
			unsigned long ptr = (unsigned long)va_arg(ap, void *);
			append_char(&out, &remaining, &written, '0');
			append_char(&out, &remaining, &written, 'x');
			append_unsigned_long(&out, &remaining, &written, ptr, 16, 0);
			break;
		}
		default:
			append_char(&out, &remaining, &written, '%');
			if (spec)
			{
				append_char(&out, &remaining, &written, spec);
			}
			break;
		}
	}
	if (size)
	{
		if (remaining)
		{
			*out = '\0';
		}
		else
		{
			buf[size - 1] = '\0';
		}
	}
	return (int)written;
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list ap;
	int len;

	va_start(ap, fmt);
	len = vsnprintf(buf, size, fmt, ap);
	va_end(ap);
	return len;
}