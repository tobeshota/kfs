#ifndef KFS_SHELL_H
#define KFS_SHELL_H

#include <kfs/stddef.h>

void shell_init(void);
void shell_run(void) __attribute__((weak));
int shell_is_initialized(void);
int shell_keyboard_handler(char c);

#endif /* KFS_SHELL_H */
