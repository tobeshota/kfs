#include <kfs/errno.h>
#include <kfs/pty.h>
#include <kfs/sched.h>
#include <kfs/string.h>

/* PTY(Pseudo Terminal) の片方向バッファ容量（byte） */
#define PTY_BUF_SIZE 512

/* PTY(Pseudo Terminal) master/slave の1ペア状態 */
struct pty_pair
{
	int in_use;						   /* 使用中フラグ */
	char m2s_buf[PTY_BUF_SIZE];		   /* master→slave バッファ */
	unsigned int m2s_len;			   /* master→slave バッファ内の有効データ長 */
	char s2m_buf[PTY_BUF_SIZE];		   /* slave→master バッファ */
	unsigned int s2m_len;			   /* slave→master バッファ内の有効データ長 */
	struct task_struct *master_waiter; /* master 側で slave からのデータを待っているタスク */
	struct task_struct *slave_waiter;  /* slave 側で master からのデータを待っているタスク */
	pid_t slave_foreground_pgrp; /* slave 側の所属フォアグラウンドプロセスグループ（tty制御用） */
};

/* 利用可能な PTY ペア一覧 */
static struct pty_pair pty_pairs[PTY_MAX_PAIRS];

/** fd から PTY スロット番号を求める
 * @param fd 対象ファイルディスクリプタ
 * @param is_master master 側なら1、slave 側なら0を返す出力先（NULL可）
 * @return スロット番号（0以上），fdがPTYでない場合は-1
 */
static int pty_slot_from_fd(int fd, int *is_master)
{
	/* fd が PTY master の範囲内か確認 */
	if (fd >= PTY_MASTER_FD_BASE && fd < PTY_MASTER_FD_BASE + PTY_MAX_PAIRS)
	{
		if (is_master)
		{
			/* master 側であることを示すフラグを設定 */
			*is_master = 1;
		}
		/* master 側のスロット番号を返す */
		return fd - PTY_MASTER_FD_BASE;
	}

	/* fd が PTY slave の範囲内か確認 */
	if (fd >= PTY_SLAVE_FD_BASE && fd < PTY_SLAVE_FD_BASE + PTY_MAX_PAIRS)
	{
		if (is_master)
		{
			/* slave 側であることを示すフラグを設定 */
			*is_master = 0;
		}
		/* slave 側のスロット番号を返す */
		return fd - PTY_SLAVE_FD_BASE;
	}

	/* fd が PTY でない場合 */
	return -1;
}

/* 全 PTY 状態を初期化する */
void pty_reset(void)
{
	for (int i = 0; i < PTY_MAX_PAIRS; ++i)
	{
		pty_pairs[i].in_use = 0;
		pty_pairs[i].m2s_len = 0;
		pty_pairs[i].s2m_len = 0;
		pty_pairs[i].master_waiter = NULL;
		pty_pairs[i].slave_waiter = NULL;
		pty_pairs[i].slave_foreground_pgrp = 0;
	}
}

/** 新しい PTY master/slave ペアを確保する
 * @param master_fd 生成された master fd の格納先
 * @param slave_fd 生成された slave fd の格納先
 * @return 0: 成功，-EINVAL: 引数不正，-EAGAIN: 空きスロットなし
 */
int pty_open(int *master_fd, int *slave_fd)
{
	if (!master_fd || !slave_fd)
	{
		return -EINVAL;
	}

	for (int i = 0; i < PTY_MAX_PAIRS; ++i)
	{
		/* 空きスロットを探す */
		if (!pty_pairs[i].in_use)
		{
			pty_pairs[i].in_use = 1;
			pty_pairs[i].m2s_len = 0;
			pty_pairs[i].s2m_len = 0;
			pty_pairs[i].master_waiter = NULL;
			pty_pairs[i].slave_waiter = NULL;
			pty_pairs[i].slave_foreground_pgrp = 0;
			*master_fd = PTY_MASTER_FD_BASE + i; /* master fd を設定 */
			*slave_fd = PTY_SLAVE_FD_BASE + i;	 /* slave fd を設定 */
			return 0;
		}
	}

	return -EAGAIN;
}

/** fd が master fd かどうか判定する
 * @param fd 判定対象 fd
 * @return master fd なら1、それ以外は0
 */
int pty_is_master_fd(int fd)
{
	return pty_slot_from_fd(fd, NULL) >= 0 && fd >= PTY_MASTER_FD_BASE && fd < PTY_MASTER_FD_BASE + PTY_MAX_PAIRS;
}

/** fd が slave fd かどうか判定する
 * @param fd 判定対象 fd
 * @return slave fd なら1、それ以外は0
 */
int pty_is_slave_fd(int fd)
{
	return pty_slot_from_fd(fd, NULL) >= 0 && fd >= PTY_SLAVE_FD_BASE && fd < PTY_SLAVE_FD_BASE + PTY_MAX_PAIRS;
}

/** fd が PTY fd かどうか判定する
 * @param fd 判定対象 fd
 * @return PTY fd なら1、それ以外は0
 */
int pty_is_fd(int fd)
{
	return pty_slot_from_fd(fd, NULL) >= 0;
}

/** PTY からデータを読み取る
 * @param fd 読み取り元 fd（master または slave）
 * @param buf 読み取り先バッファ
 * @param size 読み取り上限バイト数
 * @return 読み取ったバイト数，エラー時は負値
 * @note データが無い場合は TASK_INTERRUPTIBLE で待機する
 */
long pty_read(int fd, char *buf, unsigned int size)
{
	int is_master;
	int slot = pty_slot_from_fd(fd, &is_master);

	if (slot < 0 || !buf)
	{
		return -EINVAL;
	}
	if (size == 0)
	{
		return 0;
	}

	struct pty_pair *pair = &pty_pairs[slot];
	if (!pair->in_use)
	{
		return -EBADF;
	}

	char *src = is_master ? pair->s2m_buf : pair->m2s_buf;
	unsigned int *src_len = is_master ? &pair->s2m_len : &pair->m2s_len;
	struct task_struct **waiter = is_master ? &pair->master_waiter : &pair->slave_waiter;

	while (1)
	{
		__asm__ volatile("cli"); /* 割り込み禁止 */
		if (*src_len > 0)
		{
			/* データがある場合は読み取る */
			unsigned int n = (*src_len < size) ? *src_len : size;
			memmove(buf, src, n);
			if (*src_len > n)
			{
				memmove(src, src + n, *src_len - n);
			}
			*src_len -= n;
			__asm__ volatile("sti"); /* 割り込み許可 */
			return (long)n;
		}

		/* データが無い場合は TASK_INTERRUPTIBLE で待機する
		 * 待機から戻るのは，相手側がpty_write()でwake_up_process()を呼んだとき */
		*waiter = current;
		current->__state = TASK_INTERRUPTIBLE;

		__asm__ volatile("sti"); /* 割り込み許可 */
		schedule();
	}
}

/** PTY にデータを書き込む
 * @param fd 書き込み元 fd（master または slave）
 * @param buf 書き込み元バッファ
 * @param size 書き込み要求バイト数
 * @return 実際に書き込んだバイト数、エラー時は負値
 * @note 相手側バッファの空き容量分のみ書き込む
 */
long pty_write(int fd, const char *buf, unsigned int size)
{
	int is_master;
	int slot = pty_slot_from_fd(fd, &is_master);

	if (slot < 0 || !buf)
	{
		return -EINVAL;
	}
	if (size == 0)
	{
		return 0;
	}

	struct pty_pair *pair = &pty_pairs[slot];
	if (!pair->in_use)
	{
		return -EBADF;
	}

	char *dst = is_master ? pair->m2s_buf : pair->s2m_buf;
	unsigned int *dst_len = is_master ? &pair->m2s_len : &pair->s2m_len;

	__asm__ volatile("cli"); /* 割り込み禁止 */
	unsigned int space = PTY_BUF_SIZE - *dst_len;
	unsigned int n = (size < space) ? size : space;

	/* データがある場合は書き込む */
	if (n > 0)
	{
		memmove(dst + *dst_len, buf, n);
		*dst_len += n;
	}

	struct task_struct *peer_waiter = is_master ? pair->slave_waiter : pair->master_waiter;

	/* 相手側が待機している場合は起こし，待機状態をクリアする */
	if (peer_waiter)
	{
		wake_up_process(peer_waiter);
		if (is_master)
		{
			pair->slave_waiter = NULL;
		}
		else
		{
			pair->master_waiter = NULL;
		}
	}
	__asm__ volatile("sti"); /* 割り込み許可 */

	return (long)n;
}

/** PTY slave の foreground process group を取得する
 * @param slave_fd PTY slave fd
 * @return foreground pgrp、失敗時は -ENOTTY
 */
pid_t pty_get_foreground_pgrp_for_slave_fd(int slave_fd)
{
	int slot = pty_slot_from_fd(slave_fd, NULL);

	if (slot < 0 || !pty_is_slave_fd(slave_fd) || !pty_pairs[slot].in_use)
	{
		return -ENOTTY;
	}

	return pty_pairs[slot].slave_foreground_pgrp;
}

/** PTY slave の foreground process group を設定する
 * @param slave_fd PTY slave fd
 * @param pgrp 設定する process group id
 * @return 0: 成功，-ENOTTY: slave fd として不正
 */
int pty_set_foreground_pgrp_for_slave_fd(int slave_fd, pid_t pgrp)
{
	int slot = pty_slot_from_fd(slave_fd, NULL);

	if (slot < 0 || !pty_is_slave_fd(slave_fd) || !pty_pairs[slot].in_use)
	{
		return -ENOTTY;
	}

	pty_pairs[slot].slave_foreground_pgrp = pgrp;
	return 0;
}
