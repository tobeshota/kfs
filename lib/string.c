/* Minimal string helpers modelled after early Linux kernel style.
 * Only the subset we currently need is provided.
 * Notes:
 *  - No special handling for overlapping regions (UB mirrors standard libc)
 *  - Keep implementations simple and inline-friendly for -O2.
 */
#include <linux/string.h>

size_t strlen(const char *s)
{
    const char *p = s;
    while (*p)
        p++;
    return (size_t)(p - s);
}

char *strcpy(char *dst, const char *src)
{
    char *ret = dst;
    while ((*dst++ = *src++)) { /* copy including final NUL */ }
    return ret;
}
