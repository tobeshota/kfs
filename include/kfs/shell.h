#ifndef KFS_SHELL_H
#define KFS_SHELL_H

#include <kfs/pid.h>
#include <kfs/stddef.h>

typedef void (*shell_cmd_fn)(void *args);

/* コマンドの実行モード */
enum shell_cmd_mode
{
	SHELL_CMD_BUILTIN = 1,	/* 組み込みコマンド（シェルプロセス内で実行） */
	SHELL_CMD_EXTERNAL = 2, /* 外部コマンド（新しいプロセスで実行） */
};

void shell_init(void);
void shell_run(void) __attribute__((weak));
int shell_is_initialized(void);
int shell_keyboard_handler(char c);
void cmd_register(const char *name, shell_cmd_fn fn, enum shell_cmd_mode mode);
int cmd_lookup(const char *cmd, shell_cmd_fn *fn, const char **args, enum shell_cmd_mode *mode);
void cmd_list(void);
void cmd_list_by_mode(enum shell_cmd_mode mode);
void cmd_registry_init(void);
int shell_jobs_list(void);
int shell_jobs_fg(int job_id);
int shell_jobs_bg(int job_id);

#endif /* KFS_SHELL_H */
