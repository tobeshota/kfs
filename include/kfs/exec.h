#ifndef _KFS_EXEC_H
#define _KFS_EXEC_H

void __attribute__((noreturn)) exec_fn(void (*fn)(void *), void *arg);

#endif /* _KFS_EXEC_H */
