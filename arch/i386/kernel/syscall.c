#include <kfs/console.h>
#include <kfs/errno.h>
#include <kfs/mman.h>
#include <kfs/printk.h>
#include <kfs/psg.h>
#include <kfs/serial.h>
#include <kfs/signal.h>
#include <kfs/stddef.h>
#include <kfs/sys.h>
#include <kfs/syscall.h>
#include <kfs/wait.h>

extern pid_t do_fork(unsigned long user_eip, unsigned long user_esp);
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
static long do_sys_sched_setscheduler(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg4;
	(void)arg5;
	return (long)sys_sched_setscheduler((pid_t)arg1, (int)arg2, (int)arg3);
}

/* sys_sched_getscheduler(pid) のラッパー */
static long do_sys_sched_getscheduler(long arg1, long arg2, long arg3, long arg4, long arg5)
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

/** write() システムコール
 * @param arg1 fd   1=stdout/2=stderr → VGA端末、4=シリアルCOM1
 * @param arg2 buf  書き込むデータのポインタ
 * @param arg3 count バイト数
 * @return 書き込んだバイト数、エラー時負数
 */
static long do_sys_write(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	int fd = (int)arg1;
	const char *buf = (const char *)arg2;
	size_t count = (size_t)arg3;
	size_t i;

	(void)arg4;
	(void)arg5;

	/* fdの妙合性を先に確認（bufを調べる前に行う） */
	if (fd != 1 && fd != 2 && fd != 4)
	{
		return -EBADF;
	}

	if (!buf || count == 0)
	{
		return 0;
	}

	if (fd == 1 || fd == 2)
	{
		/* stdout / stderr → VGA端末に1文字ずつ出力 */
		for (i = 0; i < count; i++)
		{
			terminal_putchar(buf[i]);
		}
		return (long)count;
	}
	if (fd == 4)
	{
		/* COM1シリアル → serial_write でバルク出力 */
		serial_write(buf, count);
		return (long)count;
	}
	return -EBADF;
}

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
	return (long)do_fork(0, 0);
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

/** msleep(ms) システムコール
 * @param arg1 スリープ時間 [ミリ秒]
 * @return 0: 成功
 */
static long do_sys_msleep(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return sys_msleep((uint32_t)arg1);
}

static long do_sys_psg_note(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg4;
	(void)arg5;
	do_psg_note((int)arg1, (uint32_t)arg2, (uint32_t)arg3);
	return 0;
}

static long do_sys_psg_stop(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	do_psg_stop((int)arg1);
	return 0;
}

/** sigreturn() システムコール
 * @brief ring-3 シグナルハンドラが return した後、sigreturn()（lib/unistd.c）から呼ばれる
 * @return 元のプロセスの eax 値（entry.S が pt_regs->eax に書く）
 */
static long do_sys_sigreturn(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg1;
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return (long)sys_sigreturn();
}

/* mmap2(addr, len, prot, flags, fd) システムコール
 * @note pgoff は MAP_ANONYMOUS では不要なため省略（syscall_fn_t は5引数）
 */
static long do_sys_mmap2(long addr, long len, long prot, long flags, long fd)
{
	return (long)sys_mmap2((unsigned long)addr, (unsigned long)len, (int)prot, (int)flags, (int)fd, 0);
}

/* munmap(addr, len) システムコール */
static long do_sys_munmap(long addr, long len, long a3, long a4, long a5)
{
	(void)a3;
	(void)a4;
	(void)a5;
	return (long)sys_munmap((unsigned long)addr, (unsigned long)len);
}

static syscall_fn_t sys_call_table[NR_syscalls] = {
	[__NR_exit] = (syscall_fn_t)do_sys_exit,
	[__NR_fork] = do_sys_fork,
	[__NR_write] = do_sys_write,
	[__NR_wait] = do_sys_wait,
	[__NR_getuid] = do_sys_getuid,
	[__NR_kill] = do_sys_kill,
	[__NR_signal] = do_sys_signal,
	[__NR_sched_setscheduler] = do_sys_sched_setscheduler,
	[__NR_sched_getscheduler] = do_sys_sched_getscheduler,
	[__NR_msleep] = do_sys_msleep,
	[__NR_psg_note] = do_sys_psg_note,
	[__NR_psg_stop] = do_sys_psg_stop,
	[__NR_sigreturn] = do_sys_sigreturn,
	[__NR_munmap] = do_sys_munmap,
	[__NR_mmap2] = do_sys_mmap2,
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
