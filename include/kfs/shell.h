#ifndef KFS_SHELL_H
#define KFS_SHELL_H

#include <kfs/pid.h>
#include <kfs/stddef.h>

typedef void (*shell_builtin_fn)(void *args);

void shell_init(void);
void shell_run(void) __attribute__((weak));
int shell_is_initialized(void);
int shell_keyboard_handler(char c);
void builtin_register(const char *name, shell_builtin_fn fn);
int lookup_builtin(const char *cmd, shell_builtin_fn *fn, const char **args);
void shell_builtin_list(void);
void shell_builtins_init(void);

#endif /* KFS_SHELL_H */
