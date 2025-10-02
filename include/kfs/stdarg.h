#ifndef _KFS_STDARG_H
#define _KFS_STDARG_H

/* Minimal stdarg.h using GCC builtins for freestanding environment */

typedef __builtin_va_list va_list;

#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_end(ap) __builtin_va_end(ap)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#ifdef __builtin_va_copy
#define va_copy(dest, src) __builtin_va_copy(dest, src)
#else
#define va_copy(dest, src) ((dest) = (src))
#endif

#endif /* _KFS_STDARG_H */
