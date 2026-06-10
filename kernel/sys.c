#include <kfs/capability.h>
#include <kfs/console.h>
#include <kfs/errno.h>
#include <kfs/ioctl.h>
#include <kfs/keyboard.h>
#include <kfs/list.h>
#include <kfs/neofetch.h>
#include <kfs/panic.h>
#include <kfs/pid.h>
#include <kfs/prctl.h>
#include <kfs/ps.h>
#include <kfs/pty.h>
#include <kfs/sched.h>
#include <kfs/serial.h>
#include <kfs/signal.h>
#include <kfs/string.h>
#include <kfs/sys.h>
#include <kfs/timer.h>
#include <kfs/tty.h>

#include <asm-i386/page.h>

#define BYTES_PER_MIB (1024UL * 1024UL)

extern unsigned long total_pages;
extern unsigned long nr_free_pages;
extern unsigned long kernel_end_pfn;

/** @brief task_struct の状態から ps 表示用の1文字を返す
 * @param task 対象の task_struct
 * @return 状態文字 ('R','S','D','T','Z','X' など)、不明時は '?'
 */
static char ps_state_char(const struct task_struct *task)
{
	if (task->exit_state == EXIT_ZOMBIE)
	{
		return 'Z';
	}
	if (task->exit_state == EXIT_DEAD)
	{
		return 'X';
	}
	if (task->__state == TASK_RUNNING)
	{
		return 'R';
	}
	if (task->__state & TASK_INTERRUPTIBLE)
	{
		return 'S';
	}
	if (task->__state & TASK_UNINTERRUPTIBLE)
	{
		return 'D';
	}
	if (task->__state & __TASK_STOPPED)
	{
		return 'T';
	}
	return '?';
}

/** @brief ps_snapshot の内部コンテキスト
 * @details コールバックへ渡され、書き込み先配列・上限・現在の件数を保持する
 * @note ctx は context の略
 */
struct ps_snapshot_ctx
{
	struct kfs_ps_entry *entries;
	size_t max_entries;
	size_t count;
};

/* task の所属コンソール番号を ps 表示用TTY文字列へ変換する。 */
static void ps_fill_tty(char *dst, size_t dst_len, size_t tty_console)
{
	if (!dst || dst_len == 0)
	{
		return;
	}

	if (tty_console >= kfs_terminal_console_count())
	{
		dst[0] = '-';
		if (dst_len > 1)
		{
			dst[1] = '\0';
		}
		return;
	}

	/* 端末番号はユーザー表示で 1 始まりにする（tty1, tty2, ...）。 */
	size_t n = tty_console + 1;
	char digits[20];
	size_t digits_len = 0;

	while (n > 0 && digits_len < sizeof(digits))
	{
		digits[digits_len++] = (char)('0' + (n % 10));
		n /= 10;
	}

	if (dst_len < 5)
	{
		dst[0] = '-';
		if (dst_len > 1)
		{
			dst[1] = '\0';
		}
		return;
	}

	dst[0] = 't';
	dst[1] = 't';
	dst[2] = 'y';

	size_t pos = 3;
	while (digits_len > 0 && pos + 1 < dst_len)
	{
		dst[pos++] = digits[--digits_len];
	}
	dst[pos] = '\0';
}

/** @brief タスク走査コールバック — 各タスク情報を `kfs_ps_entry` に詰める
 * @param task 現在のタスク
 * @param ctx  `struct ps_snapshot_ctx *` にキャスト可能なコンテキスト
 * @return 0=継続, 正数=走査中断（バッファ満杯等）
 * @note 出力バッファの上限に達すると正値を返して走査を中断する。
 */
static int ps_snapshot_collect(struct task_struct *task, void *ctx)
{
	struct ps_snapshot_ctx *snapshot = (struct ps_snapshot_ctx *)ctx;
	struct kfs_ps_entry *entry;

	if (snapshot->count >= snapshot->max_entries)
	{
		/* バッファが満杯なら走査を中断する（呼び出し元で件数を確認） */
		return 1;
	}

	entry = &snapshot->entries[snapshot->count++];
	entry->pid = task->pid;
	entry->ppid = task->parent ? task->parent->pid : 0;
	ps_fill_tty(entry->tty, sizeof(entry->tty), task->tty_console);
	/* 現在はダミーの TIME 表示。将来 jiffies -> hh:mm:ss 変換を入れる */
	entry->time[0] = '0';
	entry->time[1] = ':';
	entry->time[2] = '0';
	entry->time[3] = '0';
	entry->time[4] = '\0';
	entry->stat[0] = ps_state_char(task);
	entry->stat[1] = '\0';
	strncpy(entry->cmd, task->comm, sizeof(entry->cmd));
	entry->cmd[sizeof(entry->cmd) - 1] = '\0';
	return 0;
}

long sys_ps_snapshot(struct kfs_ps_entry *entries, size_t max_entries)
{
	struct ps_snapshot_ctx ctx;

	if (!entries && max_entries != 0)
	{
		return -EINVAL;
	}

	ctx.entries = entries;
	ctx.max_entries = max_entries;
	ctx.count = 0;

	if (task_for_each(ps_snapshot_collect, &ctx) < 0)
	{
		return -EINVAL;
	}

	return (long)ctx.count;
}

int sys_neofetch_info(struct kfs_neofetch_info *info)
{
	if (!info)
	{
		return -EINVAL;
	}

	info->total_mem_mib = (total_pages * PAGE_SIZE) / BYTES_PER_MIB;
	info->free_mem_mib = (nr_free_pages * PAGE_SIZE) / BYTES_PER_MIB;
	info->used_mem_mib = info->total_mem_mib - info->free_mem_mib;
	info->kernel_mem_mib = (kernel_end_pfn * PAGE_SIZE) / BYTES_PER_MIB;
	return 0;
}

/* 現在のプロセスの実ユーザー ID を返す */
int sys_getuid(void)
{
	return (int)current->uid.val;
}

/** 現在のプロセスの UID を変更する
 * @param uid 設定する UID
 * @return 0: 成功, -EPERM: CAP_SETUID がない
 */
int sys_setuid(uid_t uid)
{
	if (!capable(CAP_SETUID))
	{
		return -EPERM;
	}
	current->uid.val = uid;
	current->euid.val = uid;
	return 0;
}

/** 指定 PID のタスクの Capability セットを取得する
 * @param pid         対象 PID（0の場合呼び出し元プロセス）
 * @param effective   有効 Capability の格納先
 * @param permitted   許可 Capability の格納先（NULL で省略可）
 * @param inheritable 継承 Capability の格納先（NULL で省略可）
 * @return 0: 成功, -ESRCH: PID が見つからない, -EINVAL: 引数不正
 */
int sys_capget(pid_t pid, kernel_cap_t *effective, kernel_cap_t *permitted, kernel_cap_t *inheritable)
{
	struct task_struct *tsk = (pid == 0) ? current : find_task_by_pid(pid);

	if (!tsk)
	{
		return -ESRCH;
	}
	return cap_capget(tsk, effective, permitted, inheritable);
}

/** 指定 PID のタスクの有効 Capability セットを設定する
 * @param pid         対象 PID（0の場合呼び出し元プロセス）
 * @param effective   設定する有効 Capability
 * @param permitted   設定する許可 Capability（現在は無視）
 * @param inheritable 設定する継承 Capability（現在は無視）
 * @return 0: 成功, -ESRCH: PID が見つからない, -EINVAL/-EPERM: cap_capset に準じる
 */
int sys_capset(pid_t pid, const kernel_cap_t *effective, const kernel_cap_t *permitted, const kernel_cap_t *inheritable)
{
	struct task_struct *tsk = (pid == 0) ? current : find_task_by_pid(pid);

	if (!tsk)
	{
		return -ESRCH;
	}
	return cap_capset(tsk, effective, permitted, inheritable);
}

/** スケジューリングポリシーとリアルタイム優先度を設定する
 * @param pid      対象 PID（0の場合呼び出し元プロセス）
 * @param policy   設定するポリシー（SCHED_*）
 * @param priority RT 優先度（SCHED_FIFO/RR/DEADLINE 用、非 RT ポリシーでは 0 のみ有効）
 * @return 0: 成功, -ESRCH: PID 未存在, -EINVAL: 不正ポリシーまたは非 RT に priority != 0, -EPERM: 権限不足
 */
int sys_sched_setscheduler(pid_t pid, int policy, int priority)
{
	struct task_struct *tsk = (pid == 0) ? current : find_task_by_pid(pid);

	if (!tsk)
	{
		return -ESRCH;
	}

	/* 設定するpolicyが有効（SCHED_*）であるか確認 */
	if (policy != SCHED_NORMAL && policy != SCHED_FIFO && policy != SCHED_RR && policy != SCHED_BATCH &&
		policy != SCHED_IDLE && policy != SCHED_DEADLINE && policy != SCHED_PURE_RR)
	{
		return -EINVAL;
	}

	/* 非 RT ポリシーに priority != 0 は不正（Linux 6.18 準拠） */
	if (priority != 0 && (policy != SCHED_FIFO && policy != SCHED_RR && policy != SCHED_DEADLINE))
	{
		return -EINVAL;
	}

	/* リアルタイム系ポリシー（FIFO/RR/DEADLINE）は CAP_SYS_NICE が必要 */
	if ((policy == SCHED_FIFO || policy == SCHED_RR || policy == SCHED_DEADLINE) && !capable(CAP_SYS_NICE))
	{
		return -EPERM;
	}

	tsk->policy = (unsigned int)policy;
	tsk->rt_priority = priority;
	return 0;
}

/** スケジューリングポリシーを取得する
 * @param pid 対象 PID（0の場合呼び出し元プロセス）
 * @return ポリシー値（SCHED_*）: 成功, -ESRCH: PID 未存在
 */
int sys_sched_getscheduler(pid_t pid)
{
	struct task_struct *tsk = (pid == 0) ? current : find_task_by_pid(pid);

	if (!tsk)
	{
		return -ESRCH;
	}

	return (int)tsk->policy;
}

long sys_prctl(int option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5)
{
	(void)arg3;
	(void)arg4;
	(void)arg5;

	if (option == PR_SET_NAME)
	{
		strncpy(current->comm, (const char *)arg2, sizeof(current->comm) - 1);
		current->comm[sizeof(current->comm) - 1] = '\0';
		return 0;
	}
	return -ENOSYS; /* 未実装 */
}

/** ms ミリ秒スリープする
 * @param ms スリープ時間 [ミリ秒]（HZ=1000 なので ms == tick 数）
 * @return 残り tick 数（満了時 0、シグナル等で早期復帰時は正の値）
 */
long sys_msleep(uint32_t ms)
{
	if (ms == 0)
	{
		return 0;
	}
	return schedule_timeout((long)ms);
}

struct pgrp_scan_ctx
{
	pid_t pgrp;
	pid_t session;
	int found;
};

static int find_matching_pgrp_in_session(struct task_struct *task, void *ctx)
{
	struct pgrp_scan_ctx *scan = (struct pgrp_scan_ctx *)ctx;

	if (task->pgrp == scan->pgrp && task->session == scan->session)
	{
		scan->found = 1;
		return 1;
	}
	return 0;
}

/** 指定session内にプロセスグループpgrpが存在するか確認する
 * @return 存在する場合は1、存在しない場合は0
 */
static int process_group_exists_in_session(pid_t session, pid_t pgrp)
{
	struct pgrp_scan_ctx scan = {
		.pgrp = pgrp,
		.session = session,
		.found = 0,
	};

	(void)task_for_each(find_matching_pgrp_in_session, &scan);
	return scan.found;
}

/** 引数pidで指定されたプロセスのプロセスグループIDをgpidに設定する
 * @param pid 対象 PID（0の場合は current を意味する）
 * @param pgid 設定するプロセスグループID（0の場合は pgid = pid を意味する）
 * @return 成功: 0、PIDが見つからない: -ESRCH、引数不正: -EINVAL
 * @example
 * setpgid(0, 0);  // 自身をプロセスグループPID番のプロセスグループリーダーにする
 *                 // （自身のPGIDを PGID == PID となるように設定する）
 *
 * @note 現時点では最小実装として呼び出し元がタスク自体，または対象タスクの親である場合のみ変更を許可する
 */
int sys_setpgid(pid_t pid, pid_t pgid)
{
	if (pid == 0)
	{
		pid = current->pid;
	}
	if (pgid == 0)
	{
		pgid = pid;
	}

	struct task_struct *tsk = find_task_by_pid(pid);
	if (!tsk)
	{
		return -ESRCH;
	}

	if (pgid <= 0)
	{
		return -EINVAL;
	}

	/* 呼び出し元がタスク自体，または対象タスクの親である場合のみ変更を許可する */
	if (pid != current->pid && tsk->parent != current)
	{
		return -ESRCH;
	}

	if (tsk->session != current->session)
	{
		return -EPERM;
	}

	if (tsk->pid == tsk->session)
	{
		return -EPERM;
	}

	if (pgid != pid && !process_group_exists_in_session(tsk->session, pgid))
	{
		return -EPERM;
	}

	tsk->pgrp = pgid;
	return 0;
}

/** 引数pidで指定されたプロセスのプロセスグループIDを返す
 * @param pid 対象 PID（0の場合は current を意味する）
 * @return 成功時: pgrp，PIDが見つからない場合: -ESRCH
 */
pid_t sys_getpgid(pid_t pid)
{
	if (pid == 0)
	{
		return current->pgrp;
	}

	struct task_struct *tsk = find_task_by_pid(pid);
	if (!tsk)
	{
		return -ESRCH;
	}
	return tsk->pgrp;
}

/** 呼び出しプロセスのプロセスグループIDを返す
 * @note getpgid(0) と同じ動作
 */
pid_t sys_getpgrp(void)
{
	return current->pgrp;
}

/** セッションを作成し，呼び出し元プロセスをセッションリーダーかつプロセスグループリーダーにする
 * @return 成功時: 新しい session ID (= pid)，失敗時: -EPERM
 */
pid_t sys_setsid(void)
{
	if (process_group_exists_in_session(current->session, current->pid))
	{
		/* セッションリーダーが既に存在する場合はエラー */
		return -EPERM;
	}

	/** 新しいセッションを作成する
	 * @brief 新しいセッションを作成するとは，task_structのsessionメンバに
	 *        他のtask_structのsessionメンバには振られていない新しいID（ここではPIDと等しいID）を
	 *        振ることである．
	 * @note  PIDと等しいSIDのセッションを作成するため，
	 *        呼び出し元プロセスはセッションリーダーになる．
	 * @note  PIDと等しいPGIDのプロセスグループを作成するため，
	 *        呼び出し元プロセスはプロセスグループリーダーになる．
	 */
	current->session = current->pid;
	current->pgrp = current->pid;

	return current->session;
}

/** 現在の端末のフォアグラウンドプロセスグループIDを返す
 * @param fd 端末のファイルディスクリプタ
 * @return フォアグラウンドプロセスグループID
 * @note 現在は fd を無視した簡易実装である
 */
pid_t sys_tcgetpgrp(int fd)
{
	if (fd == 0)
	{
		if (current->tty_console >= kfs_terminal_console_count())
		{
			return -ENOTTY;
		}
		return kfs_terminal_get_foreground_pgrp_for_console(current->tty_console);
	}

	if (pty_is_slave_fd(fd))
	{
		return pty_get_foreground_pgrp_for_slave_fd(fd);
	}

	return -ENOTTY;
}

/** 現在の端末のフォアグラウンドプロセスグループIDをpgrpに設定する
 * @param fd 端末のファイルディスクリプタ
 * @param pgrp 設定するフォアグラウンドプロセスグループID
 * @return 0: 成功，-EINVAL: 不正なプロセスグループID
 * @note 現在は fd を無視した簡易実装である
 */
int sys_tcsetpgrp(int fd, pid_t pgrp)
{
	if (pgrp <= 0)
	{
		return -EINVAL;
	}
	if (!process_group_exists_in_session(current->session, pgrp))
	{
		return -EPERM;
	}

	/* fdが0の場合，
	 * 呼び出し元プロセスの所属仮想コンソールの
	 * フォアグラウンドプロセスグループを設定する */
	if (fd == 0)
	{
		if (current->tty_console >= kfs_terminal_console_count())
		{
			return -ENOTTY;
		}
		return kfs_terminal_set_foreground_pgrp_for_console(current->tty_console, pgrp);
	}

	if (pty_is_slave_fd(fd))
	{
		return pty_set_foreground_pgrp_for_slave_fd(fd, pgrp);
	}

	return -ENOTTY;
}

/* 呼び出しプロセスの所属仮想コンソール番号を返す（0始まり） */
int sys_ttynr(void)
{
	return (int)current->tty_console;
}

/** プロセスが TTY に対してバックグラウンドで動作しているか判定する
 * @return 1: バックグラウンドプロセス（fg pgrp と異なる）, 0: フォアグラウンドまたは ctty 未設定
 * @note orphaned process group（fg pgrp = 0）は対象外とする
 */
static int is_background_tty_process(void)
{
	if (current->tty_console >= kfs_terminal_console_count())
	{
		return 0; /* ctty 未設定 */
	}

	/* 呼び出し元プロセスの所属する制御端末のフォアグラウンドプロセスグループを取得する */
	pid_t fg = kfs_terminal_get_foreground_pgrp_for_console(current->tty_console);
	if (fg == 0)
	{
		return 0; /* orphaned process group */
	}

	/* 呼び出し元プロセスのプロセスグループcurrent->pgrpが，
	 * 呼び出し元プロセスの所属する制御端末のフォアグラウンドプロセスグループと
	 * 異なることを確認する */
	return current->pgrp != fg;
}

/** stdout/stderr への書き込みを VGA + COM1 の両方に tee する
 * @param fd    1=stdout/2=stderr → VGA端末 + COM1 両方、4=COM1 のみ
 * @param buf   書き込むバッファ
 * @param count バイト数
 * @return 書き込んだバイト数、エラー時負数
 */
long sys_write(int fd, const char *buf, size_t count)
{
	if (pty_is_fd(fd))
	{
		return pty_write(fd, buf, (unsigned int)count);
	}

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
		/** バックグラウンドプロセスが TTY へ書き込もうとした場合は SIGTTOU を送信する
		 * @brief 呼び出し元プロセスのプロセスグループが
		 *        フォアグラウンドプロセスグループでない場合，
		 *        呼び出し元プロセスに対してSIGTTOUを送信する
		 */
		if (is_background_tty_process())
		{
			send_signal(SIGTTOU, current);
			return -EINTR;
		}

		/* stdout/stderr: VGA端末 と COM1 の両方に出力（tee） */
		terminal_write_console(current->tty_console, buf, count);
		serial_write(buf, count);
	}
	else
	{
		/* fd==4: COM1 のみ */
		serial_write(buf, count);
	}
	return (long)count;
}

long sys_openpty(int *master_fd, int *slave_fd)
{
	return (long)pty_open(master_fd, slave_fd);
}

int sys_kbd_set_layout(kbd_layout_t layout)
{
	if (layout != KBD_LAYOUT_QWERTY && layout != KBD_LAYOUT_AZERTY)
	{
		return -EINVAL;
	}
	kfs_keyboard_set_layout(layout);
	return 0;
}

long sys_kbd_read_event(struct kfs_keyboard_raw_event *event)
{
	if (!event)
	{
		return -EINVAL;
	}
	return kfs_keyboard_read_event(event);
}

int sys_kbd_clear_events(void)
{
	kfs_keyboard_clear_events();
	return 0;
}

int sys_kbd_set_raw_mode(int enabled)
{
	kfs_keyboard_set_raw_mode(enabled);
	return 0;
}

void sys_panic(void)
{
	panic("panic requested from user space");
}

/** デバイスを制御する
 * @param fd  ファイルディスクリプタ（KFS では現在小複数指定）
 * @param cmd コマンド
 * @param arg コマンドに応じた引数
 * @return 0: 成功, -EINVAL: 範囲外, -ENOTTY: 未サポートコマンド
 */
long sys_ioctl(int fd, unsigned int cmd, unsigned long arg)
{

	if (cmd == TIOCSCTTY)
	{
		/* arg は接続するコンソール番号（現在/dev/ttyの代わり） */
		if (arg >= kfs_terminal_console_count())
		{
			return -EINVAL;
		}
		/* 端末argを呼び出し元プロセスの制御端末にする */
		current->tty_console = (size_t)arg;
		return 0;
	}

	if (cmd == TCGETS || cmd == TCSETS)
	{
		if (fd != 0 || current->tty_console >= kfs_terminal_console_count())
		{
			return -ENOTTY;
		}
		if (!arg)
		{
			return -EINVAL;
		}

		if (cmd == TCGETS)
		{
			/* 端末の属性を取得する */
			return tty_get_termios_for_console(current->tty_console, (struct termios *)arg);
		}

		/* 端末の属性を設定する */
		return tty_set_termios_for_console(current->tty_console, (const struct termios *)arg);
	}

	return -ENOTTY; /* 未サポートコマンド */
}
