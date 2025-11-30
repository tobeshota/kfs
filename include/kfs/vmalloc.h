#ifndef _KFS_VMALLOC_H
#define _KFS_VMALLOC_H

#include <kfs/stddef.h>

void vmalloc_init(void);
void *vmalloc(unsigned long size);
void vfree(void *addr);
size_t vsize(void *addr);
void *vbrk(long increment);

#endif /* _KFS_VMALLOC_H */
