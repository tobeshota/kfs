#include <asm-i386/ptrace.h>
#include <kfs/errno.h>
#include <kfs/keyboard.h>
#include <kfs/mman.h>
#include <kfs/panic.h>
#include <kfs/printk.h>
#include <kfs/psg.h>
#include <kfs/pty.h>
#include <kfs/reboot.h>
#include <kfs/sched.h>
#include <kfs/sched_ext.h>
#include <kfs/signal.h>
#include <kfs/stddef.h>
#include <kfs/sys.h>
#include <kfs/syscall.h>
#include <kfs/tty.h>
#include <kfs/wait.h>

extern pid_t do_fork(unsigned long user_eip, unsigned long arg);
extern long kfs_keyboard_read_event(struct kfs_keyboard_raw_event *event);
extern void kfs_keyboard_clear_events(void);
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
 * @param arg1 fd    1=stdout/2=stderr → VGA端末 + COM1 両方、4=COM1 のみ
 * @param arg2 buf   書き込むデータのポインタ
 * @param arg3 count バイト数
 * @return 書き込んだバイト数、エラー時負数
 */
static long do_sys_write(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg4;
	(void)arg5;
	return sys_write((int)arg1, (const char *)arg2, (size_t)arg3);
}

/* read(fd, buf, count) システムコール */
static long do_sys_read(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg4;
	(void)arg5;
	if (arg1 == 0)
	{
		return tty_read_line_for_console(current->tty_console, (char *)arg2, (unsigned int)arg3);
	}
	if (pty_is_fd((int)arg1))
	{
		return pty_read((int)arg1, (char *)arg2, (unsigned int)arg3);
	}
	return -EBADF;
}

static long do_sys_kbd_read_event(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return sys_kbd_read_event((struct kfs_keyboard_raw_event *)arg1);
}

static long do_sys_kbd_clear_events(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg1;
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return (long)sys_kbd_clear_events();
}

static long do_sys_kbd_set_raw_mode(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return (long)sys_kbd_set_raw_mode((int)arg1);
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

/** waitpid(pid, wstatus, options) システムコールラッパー */
static long do_sys_waitpid(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg4;
	(void)arg5;
	return (long)sys_waitpid((pid_t)arg1, (int *)arg2, (int)arg3);
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

static long do_sys_prctl(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg1;
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return (long)sys_prctl((int)arg1, (unsigned long)arg2, (unsigned long)arg3, (unsigned long)arg4,
						   (unsigned long)arg5);
}

/* setpgid(pid, pgid) のラッパー */
static long do_sys_setpgid(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return (long)sys_setpgid((pid_t)arg1, (pid_t)arg2);
}

/* getpgid(pid) のラッパー */
static long do_sys_getpgid(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return (long)sys_getpgid((pid_t)arg1);
}

/* getpgrp() のラッパー */
static long do_sys_getpgrp(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg1;
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return (long)sys_getpgrp();
}

/* setsid() のラッパー */
static long do_sys_setsid(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg1;
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return (long)sys_setsid();
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

static long do_sys_ps_snapshot(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return (long)sys_ps_snapshot((struct kfs_ps_entry *)arg1, (size_t)arg2);
}

static long do_sys_neofetch_info(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return (long)sys_neofetch_info((struct kfs_neofetch_info *)arg1);
}

/** sigreturn() システムコール
 * @brief ring-3 シグナルハンドラが return した後、sigreturn()（arch/i386/kernel/sigreturn.S）から呼ばれる
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

/* tcgetpgrp(fd) のラッパー */
static long do_sys_tcgetpgrp(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return (long)sys_tcgetpgrp((int)arg1);
}

/* tcsetpgrp(fd, pgrp) のラッパー */
static long do_sys_tcsetpgrp(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return (long)sys_tcsetpgrp((int)arg1, (pid_t)arg2);
}

/* ttynr() のラッパー */
static long do_sys_ttynr(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg1;
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return (long)sys_ttynr();
}

/* openpty(master_fd, slave_fd) のラッパー */
static long do_sys_openpty(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return sys_openpty((int *)arg1, (int *)arg2);
}

/* ioctl(fd, cmd, arg) システムコール */
static long do_sys_ioctl(long fd, long cmd, long arg, long a4, long a5)
{
	(void)a4;
	(void)a5;
	return sys_ioctl((int)fd, (unsigned int)cmd, (unsigned long)arg);
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

/* reboot() システムコール */
static long do_sys_reboot(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg1;
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	machine_restart();
	__builtin_unreachable();
}

/* halt() システムコール */
static long do_sys_halt(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg1;
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	__asm__ __volatile__("cli");
	clear_gp_registers();
	for (;;)
	{
		__asm__ __volatile__("hlt");
	}
	__builtin_unreachable();
}

/* kbd_set_layout(layout) システムコール */
static long do_sys_kbd_set_layout(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return (long)sys_kbd_set_layout((kbd_layout_t)arg1);
}

/* panic() システムコール */
static long __attribute__((noreturn)) do_sys_panic(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg1;
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	sys_panic();
	__builtin_unreachable();
}

/** sched_ext_load(name) syscall wrapper */
static long do_sys_sched_ext_load(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return (long)sys_sched_ext_load((const char *)arg1);
}

/** sched_ext_unload() syscall wrapper */
static long do_sys_sched_ext_unload(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg1;
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return (long)sys_sched_ext_unload();
}

/** sched_ext_status(status) syscall wrapper */
static long do_sys_sched_ext_status(long arg1, long arg2, long arg3, long arg4, long arg5)
{
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return (long)sys_sched_ext_status((struct sched_ext_status *)arg1);
}

static syscall_fn_t sys_call_table[NR_syscalls] = {
	[__NR_exit] = (syscall_fn_t)do_sys_exit,
	[__NR_fork] = do_sys_fork,
	[__NR_read] = do_sys_read,
	[__NR_write] = do_sys_write,
	[__NR_wait] = do_sys_wait,
	[__NR_getuid] = do_sys_getuid,
	[__NR_kill] = do_sys_kill,
	[__NR_signal] = do_sys_signal,
	[__NR_setsid] = do_sys_setsid,
	[__NR_sched_setscheduler] = do_sys_sched_setscheduler,
	[__NR_sched_getscheduler] = do_sys_sched_getscheduler,
	[__NR_prctl] = do_sys_prctl,
	[__NR_msleep] = do_sys_msleep,
	[__NR_psg_note] = do_sys_psg_note,
	[__NR_psg_stop] = do_sys_psg_stop,
	[__NR_ps_snapshot] = do_sys_ps_snapshot,
	[__NR_sigreturn] = do_sys_sigreturn,
	[__NR_munmap] = do_sys_munmap,
	[__NR_mmap2] = do_sys_mmap2,
	[__NR_waitpid] = do_sys_waitpid,
	[__NR_setpgid] = do_sys_setpgid,
	[__NR_getpgid] = do_sys_getpgid,
	[__NR_getpgrp] = do_sys_getpgrp,
	[__NR_tcgetpgrp] = do_sys_tcgetpgrp,
	[__NR_tcsetpgrp] = do_sys_tcsetpgrp,
	[__NR_reboot] = do_sys_reboot,
	[__NR_halt] = do_sys_halt,
	[__NR_kbd_set_layout] = do_sys_kbd_set_layout,
	[__NR_panic] = (syscall_fn_t)do_sys_panic,
	[__NR_kbd_read_event] = do_sys_kbd_read_event,
	[__NR_kbd_clear_events] = do_sys_kbd_clear_events,
	[__NR_kbd_set_raw_mode] = do_sys_kbd_set_raw_mode,
	[__NR_neofetch_info] = do_sys_neofetch_info,
	[__NR_ttynr] = do_sys_ttynr,
	[__NR_openpty] = do_sys_openpty,
	[__NR_sched_ext_load] = do_sys_sched_ext_load,
	[__NR_sched_ext_unload] = do_sys_sched_ext_unload,
	[__NR_sched_ext_status] = do_sys_sched_ext_status,
	[__NR_ioctl] = do_sys_ioctl,
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
