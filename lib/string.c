#include "linux/string.h"

size_t strlen(const char *s)
{
	const char *p = s;
	while (*p)
		p++;
	return (size_t)(p - s);
}

size_t strnlen(const char *s, size_t maxlen)
{
	const char *p = s;
	while (maxlen-- && *p)
		p++;
	return (size_t)(p - s);
}

char *strcpy(char *dst, const char *src)
{
	char *ret = dst;
	while ((*dst++ = *src++))
	{ /* copy including final NUL */
	}
	return ret;
}

char *strncpy(char *dst, const char *src, size_t count)
{
	char *ret = dst;
	while (count && *src)
	{
		*dst++ = *src++;
		count--;
	}
	while (count--)
		*dst++ = '\0';
	return ret;
}

size_t strlcpy(char *dst, const char *src, size_t size)
{
	size_t len = strlen(src);
	if (size)
	{
		size_t copy = (len >= size) ? size - 1 : len;
		for (size_t i = 0; i < copy; i++)
			dst[i] = src[i];
		dst[copy] = '\0';
	}
	return len;
}

char *strcat(char *dst, const char *src)
{
	char *ret = dst;
	while (*dst)
		dst++;
	while ((*dst++ = *src++))
	{
	}
	return ret;
}

char *strncat(char *dst, const char *src, size_t count)
{
	char *ret = dst;
	while (*dst)
		dst++;
	if (!count)
		return ret;
	while (count-- && *src)
		*dst++ = *src++;
	*dst = '\0';
	return ret;
}

int strcmp(const char *lhs, const char *rhs)
{
	while (*lhs && (*lhs == *rhs))
	{
		lhs++;
		rhs++;
	}
	return (unsigned char)*lhs - (unsigned char)*rhs;
}

int strncmp(const char *lhs, const char *rhs, size_t count)
{
	while (count && *lhs && (*lhs == *rhs))
	{
		lhs++;
		rhs++;
		count--;
	}
	if (!count)
		return 0;
	return (unsigned char)*lhs - (unsigned char)*rhs;
}

char *strchr(const char *str, int ch)
{
	char c = (char)ch;
	for (;; str++)
	{
		if (*str == c)
			return (char *)str;
		if (*str == '\0')
			return NULL;
	}
}

char *strrchr(const char *str, int ch)
{
	char c = (char)ch;
	const char *last = NULL;
	for (;; str++)
	{
		if (*str == c)
			last = str;
		if (*str == '\0')
			break;
	}
	return (char *)last;
}

char *strstr(const char *haystack, const char *needle)
{
	if (!*needle)
		return (char *)haystack;
	for (const char *h = haystack; *h; h++)
	{
		if (*h != *needle)
			continue;
		const char *p = h + 1;
		const char *q = needle + 1;
		while (*p && *q && *p == *q)
		{
			p++;
			q++;
		}
		if (!*q)
			return (char *)h;
	}
	return NULL;
}

void *memset(void *dst, int value, size_t count)
{
	unsigned char *p = (unsigned char *)dst;
	unsigned char val = (unsigned char)value;
	while (count--)
		*p++ = val;
	return dst;
}

void *memcpy(void *dst, const void *src, size_t count)
{
	unsigned char *d = (unsigned char *)dst;
	const unsigned char *s = (const unsigned char *)src;
	while (count--)
		*d++ = *s++;
	return dst;
}

void *memmove(void *dst, const void *src, size_t count)
{
	unsigned char *d = (unsigned char *)dst;
	const unsigned char *s = (const unsigned char *)src;
	if (d == s || count == 0)
		return dst;
	if (d < s)
	{
		while (count--)
			*d++ = *s++;
	}
	else
	{
		while (count--)
			d[count] = s[count];
	}
	return dst;
}

int memcmp(const void *lhs, const void *rhs, size_t count)
{
	const unsigned char *a = (const unsigned char *)lhs;
	const unsigned char *b = (const unsigned char *)rhs;
	while (count--)
	{
		unsigned char va = *a++;
		unsigned char vb = *b++;
		if (va != vb)
			return (int)va - (int)vb;
	}
	return 0;
}

void *memchr(const void *ptr, int ch, size_t count)
{
	const unsigned char *p = (const unsigned char *)ptr;
	unsigned char target = (unsigned char)ch;
	while (count--)
	{
		if (*p == target)
			return (void *)p;
		p++;
	}
	return NULL;
}
