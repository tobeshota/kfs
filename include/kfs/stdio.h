#ifndef _KFS_STDIO_H
#define _KFS_STDIO_H

#include <kfs/stdarg.h>
#include <kfs/stddef.h>

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
int snprintf(char *buf, size_t size, const char *fmt, ...);

int vdprintf(int fd, const char *fmt, va_list ap);
int dprintf(int fd, const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int printf(const char *fmt, ...);

#endif /* _KFS_STDIO_H */
