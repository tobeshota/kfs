/* Minimal string helpers (subset) */
#ifndef LINUX_STRING_H
#define LINUX_STRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

char *strcpy(char *dst, const char *src);
size_t strlen(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* LINUX_STRING_H */
