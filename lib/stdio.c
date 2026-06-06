#include <kfs/stdio.h>
#include <kfs/unistd.h>

#define KFS_STDIO_BUFFER_SIZE 1024

static char kfs_stdio_buffer[KFS_STDIO_BUFFER_SIZE];

int vdprintf(int fd, const char *fmt, va_list ap)
{
	va_list copy;
	int len;

	va_copy(copy, ap);
	len = vsnprintf(kfs_stdio_buffer, sizeof(kfs_stdio_buffer), fmt, copy);
	va_end(copy);

	if (len < 0)
	{
		return len;
	}

	unsigned int to_write = (unsigned int)len;
	if (to_write >= sizeof(kfs_stdio_buffer))
	{
		to_write = (unsigned int)(sizeof(kfs_stdio_buffer) - 1);
	}

	if (to_write > 0)
	{
		int w = write(fd, kfs_stdio_buffer, to_write);
		if (w < 0)
		{
			return w;
		}
	}

	return len;
}

int dprintf(int fd, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vdprintf(fd, fmt, ap);
	va_end(ap);
	return ret;
}

int vprintf(const char *fmt, va_list ap)
{
	return vdprintf(1, fmt, ap);
}

int printf(const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vdprintf(1, fmt, ap);
	va_end(ap);
	return ret;
}
