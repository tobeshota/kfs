#ifndef KFS_SHELL_H
#define KFS_SHELL_H

#include <kfs/pid.h>
#include <kfs/stddef.h>

#define SHELL_NAME "kfs-shell" /* シェル名 */

typedef void (*shell_builtin_fn)(const char *args, int foreground);

void shell_init(void);
void shell_run(void) __attribute__((weak));
int shell_is_initialized(void);
int shell_keyboard_handler(char c);
pid_t shell_launch_ring3_job(const char *name, void (*fn)(void), int foreground);
void builtin_register(const char *name, shell_builtin_fn fn);
int lookup_builtin(const char *cmd, shell_builtin_fn *fn, const char **args);
void shell_builtin_list(void);
void shell_builtins_init(void);

#endif /* KFS_SHELL_H */
