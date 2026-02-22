#include <kfs/errno.h>
#include <kfs/printk.h>
#include <kfs/signal.h>
#include <kfs/sys.h>
#include <kfs/syscall.h>
#include <kfs/wait.h>

extern pid_t do_fork(void);
extern void sys_exit(int error_code);

/**　未実装のシステムコール用のスタブ
 * @return -ENOSYS
 * @note 関数名は system not implemented syscall の意
 */
static long sys_ni_syscall(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg1;
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return -ENOSYS;
}

/* sys_sched_setscheduler(pid, policy, priority) のラッパー */
static long do_sched_setscheduler(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg4;
	(void)arg5;
	return (long)sys_sched_setscheduler((pid_t)arg1, (int)arg2, (int)arg3);
}

/* sys_sched_getscheduler(pid) のラッパー */
static long do_sched_getscheduler(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return (long)sys_sched_getscheduler((pid_t)arg1);
}

/**　システムコールテーブル
 * システムコール番号からハンドラ関数へのマッピング
 */
typedef long (*syscall_fn_t)(long, long, long, long, long);

/** fork() システムコール
 * @return 親: 子 PID、子: 0（ret_from_fork で pt_regs.eax = 0 が設定済み）
 */
static long do_sys_fork(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg1;
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return (long)do_fork();
}

int sys_fork(void)
{
	return (int)do_fork();
}

/** exit() システムコール
 * @param arg1 終了コード
 * @note 返らない
 */
static long __attribute__((noreturn)) do_sys_exit(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	sys_exit((int)arg1);
	__builtin_unreachable();
}

/** wait() システムコール
 * @param arg1 wstatus ポインタ（NULL で無視）
 * @return 回収した子プロセスの PID、エラー時負数
 */
static long do_sys_wait(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return (long)sys_wait((int *)arg1);
}

/** getuid() システムコール
 * @return 現在のプロセスの UID
 */
static long do_sys_getuid(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg1;
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return (long)sys_getuid();
}

/** signal() システムコール
 * @param arg1 シグナル番号
 * @param arg2 ハンドラ関数ポインタ
 * @return 以前のハンドラ関数ポインタ
 */
static long do_sys_signal(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return (long)sys_signal((int)arg1, (sighandler_t)arg2);
}

/** kill() システムコール
 * @param arg1 対象 PID
 * @param arg2 シグナル番号
 * @return 成功 0、エラー負数
 */
static long do_sys_kill(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return (long)sys_kill((pid_t)arg1, (int)arg2);
}

static syscall_fn_t sys_call_table[NR_syscalls] = {
	[__NR_exit] = (syscall_fn_t)do_sys_exit,
	[__NR_fork] = do_sys_fork,
	[__NR_wait] = do_sys_wait,
	[__NR_getuid] = do_sys_getuid,
	[__NR_kill] = do_sys_kill,
	[__NR_signal] = do_sys_signal,
	[__NR_sched_setscheduler] = do_sched_setscheduler,
	[__NR_sched_getscheduler] = do_sched_getscheduler,
};

/** システムコールディスパッチャ
 * @param nr   システムコール番号(EAXから渡される)
 * @param arg1 システムコールの第1引数(EBXから渡される)
 * @param arg2 システムコールの第2引数(ECXから渡される)
 * @param arg3 システムコールの第3引数(EDXから渡される)
 * @param arg4 システムコールの第4引数(ESIから渡される)
 * @param arg5 システムコールの第5引数(EDIから渡される)
 * @return システムコールの戻り値(EAXに返される)
 * @note entry.Sのsystem_callから呼ばれる
 */
long do_syscall(long nr, long arg1, long arg2, long arg3, long arg4, long arg5)
{
	/* 無効なシステムコール番号をチェック */
	if (nr < 0 || nr >= NR_syscalls)
	{
		return -ENOSYS;
	}

	/* sys_call_tableから対応するハンドラを取得して呼び出す */
	if (!sys_call_table[nr])
	{
		return sys_ni_syscall(arg1, arg2, arg3, arg4, arg5);
	}
	return sys_call_table[nr](arg1, arg2, arg3, arg4, arg5);
}
