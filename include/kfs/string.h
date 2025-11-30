#ifndef _STRING_H_
#define _STRING_H_

#include <kfs/stddef.h>

#ifndef __KERNEL_SIZE_T_DEFINED
typedef size_t __kernel_size_t;
#define __KERNEL_SIZE_T_DEFINED
#endif

#ifndef __HAVE_ARCH_STRCPY
char *strcpy(char *dst, const char *src);
#endif
#ifndef __HAVE_ARCH_STRNCPY
char *strncpy(char *dst, const char *src, __kernel_size_t count);
#endif
#ifndef __HAVE_ARCH_STRLCPY
__kernel_size_t strlcpy(char *dst, const char *src, __kernel_size_t size);
#endif
#ifndef __HAVE_ARCH_STRCAT
char *strcat(char *dst, const char *src);
#endif
#ifndef __HAVE_ARCH_STRNCAT
char *strncat(char *dst, const char *src, __kernel_size_t count);
#endif
#ifndef __HAVE_ARCH_STRCMP
int strcmp(const char *lhs, const char *rhs);
#endif
#ifndef __HAVE_ARCH_STRNCMP
int strncmp(const char *lhs, const char *rhs, __kernel_size_t count);
#endif
#ifndef __HAVE_ARCH_STRCHR
char *strchr(const char *str, int ch);
#endif
#ifndef __HAVE_ARCH_STRRCHR
char *strrchr(const char *str, int ch);
#endif
#ifndef __HAVE_ARCH_STRSTR
char *strstr(const char *haystack, const char *needle);
#endif
#ifndef __HAVE_ARCH_STRLEN
__kernel_size_t strlen(const char *s);
#endif
#ifndef __HAVE_ARCH_STRNLEN
__kernel_size_t strnlen(const char *s, __kernel_size_t maxlen);
#endif

#ifndef __HAVE_ARCH_MEMSET
void *memset(void *dst, int value, __kernel_size_t count);
#endif
#ifndef __HAVE_ARCH_MEMCPY
void *memcpy(void *dst, const void *src, __kernel_size_t count);
#endif
#ifndef __HAVE_ARCH_MEMMOVE
void *memmove(void *dst, const void *src, __kernel_size_t count);
#endif
#ifndef __HAVE_ARCH_MEMCMP
int memcmp(const void *lhs, const void *rhs, __kernel_size_t count);
#endif
#ifndef __HAVE_ARCH_MEMCHR
void *memchr(const void *ptr, int ch, __kernel_size_t count);
#endif

#endif /* _STRING_H_ */
