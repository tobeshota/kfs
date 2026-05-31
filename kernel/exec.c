#include <kfs/exec.h>
#include <kfs/prctl.h>
#include <kfs/sched.h>
#include <kfs/string.h>
#include <kfs/unistd.h>

/* 指定した関数をユーザ空間のプロセスとして実行する */
void __attribute__((noreturn)) __exec_fn(const char *fn_name, void (*fn)(void *), void *arg)
{
	prctl(PR_SET_NAME, (unsigned long)fn_name, 0, 0, 0); /* プロセス名を設定 */
	fn(arg);
	exit(0);
	__builtin_unreachable();
}
