#ifndef _KFS_STDDEF_H
#define _KFS_STDDEF_H

/* Minimal stddef.h for freestanding kernel build */

typedef __SIZE_TYPE__ size_t;		/* Provided by GCC builtin */
typedef __PTRDIFF_TYPE__ ptrdiff_t; /* Provided by GCC builtin */
typedef __WCHAR_TYPE__ wchar_t;		/* Optional but some code may expect */

#ifndef NULL
#define NULL ((void *)0)
#endif

#define offsetof(type, member) __builtin_offsetof(type, member)

#endif /* _KFS_STDDEF_H */
