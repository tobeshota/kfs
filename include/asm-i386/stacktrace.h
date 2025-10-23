/*
 * Minimal i386 stack trace helpers
 */
#ifndef _ASM_I386_STACKTRACE_H
#define _ASM_I386_STACKTRACE_H

#include <stddef.h>

void show_stack(unsigned long *esp);
void dump_stack(void);

#endif /* _ASM_I386_STACKTRACE_H */
