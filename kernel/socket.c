#include <asm-i386/system.h>
#include <kfs/errno.h>
#include <kfs/exit.h>
#include <kfs/sched.h>
#include <kfs/signal.h>
#include <kfs/socket.h>
#include <kfs/string.h>

/* Unix socket endpointの受信状態 */
struct unix_socket_endpoint
{
	char buffer[UNIX_SOCKET_BUF_SIZE]; /* 受信バッファ */
	unsigned int length;			   /* バッファ内のデータ長 */
	struct task_struct *reader;		   /* 読み取りタスク */
};

/* 接続済みUnix socket pairの状態 */
struct unix_socket_pair
{
	int in_use;								  /* 使用中フラグ */
	pid_t owner_pid;						  /* 所有者PID */
	struct unix_socket_endpoint endpoints[2]; /* エンドポイント配列 */
};

/* 接続済みUnix socket pairの配列 */
static struct unix_socket_pair unix_socket_pairs[UNIX_SOCKET_MAX_PAIRS];

/** socket pairを解放し，待機中readerを起床する
 * @param pair 解放対象socket pair
 */
static void unix_socket_release_pair(struct unix_socket_pair *pair)
{
	/* pairが未使用の場合は何もしない */
	if (!pair->in_use)
	{
		return;
	}

	pair->in_use = 0;
	pair->owner_pid = 0;
	for (int endpoint = 0; endpoint < 2; endpoint++)
	{
		/* エンドポイントを初期化 */
		pair->endpoints[endpoint].length = 0;
		pair->endpoints[endpoint].reader = NULL;

		/** 待機中のreaderがいる場合は起床させる
		 * @brief 起床させる理由は，この関数はプロセス終了時に呼び出されるため，
		 *        readerが待機したままになると永遠に起床できなくなってしまうため．
		 */
		struct task_struct *reader = pair->endpoints[endpoint].reader;
		if (reader)
		{
			wake_up_process(reader);
		}
	}
}

/** 終了processが所有するsocket pairをすべて解放する
 * @param task 終了するprocess
 * @note この関数はunix_socket_init()でregister_exit_hook()されるため，
 *       process終了時に自動的に呼び出される．
 */
static void unix_socket_owner_exit(struct task_struct *task)
{
	unsigned long flags;

	local_irq_save(flags);
	for (int i = 0; i < UNIX_SOCKET_MAX_PAIRS; i++)
	{
		/* 終了するprocessが所有するsocket pairを解放 */
		if (unix_socket_pairs[i].in_use && unix_socket_pairs[i].owner_pid == task->pid)
		{
			unix_socket_release_pair(&unix_socket_pairs[i]);
		}
	}
	local_irq_restore(flags);
}

/** fdをsocket pair indexへ変換する
 * @param fd socket fd候補
 * @return pair index。socket fd範囲外なら-1
 */
static int unix_socket_pair_index(int fd)
{
	/* fdがUnix socket範囲内か確認 */
	if (fd < UNIX_SOCKET_FD_BASE || fd >= UNIX_SOCKET_FD_BASE + UNIX_SOCKET_FD_COUNT)
	{
		return -1;
	}

	/* この計算でpair indexが求まる理由は，
	 * fdはpairごとに2つ割り当てられており，
	 * 0番目のfdは0番目のpair，
	 * 1番目のfdは0番目のpairに対応するため． */
	return (fd - UNIX_SOCKET_FD_BASE) / 2;
}

/** fdをsocket endpoint indexへ変換する
 * @param fd socket fd
 * @return endpoint index 0または1。範囲外なら-1
 */
static int unix_socket_endpoint_index(int fd)
{
	if (unix_socket_pair_index(fd) < 0)
	{
		return -1;
	}

	/* この計算でendpoint indexが求まる理由は，
	 * fdはpairごとに2つ割り当てられており，
	 * 0番目のfdは0番目のendpoint，
	 * 1番目のfdは1番目のendpointに対応するため． */
	return (fd - UNIX_SOCKET_FD_BASE) % 2;
}

/* Unix socket subsystemを初期化する */
void unix_socket_init(void)
{
	static int exit_hook_registered;

	if (!exit_hook_registered)
	{
		register_exit_hook(unix_socket_owner_exit);
		exit_hook_registered = 1;
	}
	unix_socket_reset();
}

/* 全Unix socket pairを未使用状態へ戻す */
void unix_socket_reset(void)
{
	for (int i = 0; i < UNIX_SOCKET_MAX_PAIRS; i++)
	{
		unix_socket_pairs[i].in_use = 0;
		unix_socket_pairs[i].owner_pid = 0;
		/* 各エンドポイントを初期化 */
		for (int endpoint = 0; endpoint < 2; endpoint++)
		{
			unix_socket_pairs[i].endpoints[endpoint].length = 0;
			unix_socket_pairs[i].endpoints[endpoint].reader = NULL;
		}
	}
}

/** 接続済みUnix stream socket pairを作成する
 * @param domain AF_UNIXのみ対応
 * @param type SOCK_STREAMのみ対応
 * @param protocol 0のみ対応
 * @param sv 作成した2つのfdを書き込む配列
 * @return 0=成功，負数=エラー
 */
int unix_socket_pair(int domain, int type, int protocol, int sv[2])
{
	if (domain != AF_UNIX)
	{
		return -EAFNOSUPPORT;
	}
	if (type != SOCK_STREAM)
	{
		return -EPROTONOSUPPORT;
	}
	if (protocol != 0)
	{
		return -EPROTONOSUPPORT;
	}
	if (!sv)
	{
		return -EINVAL;
	}

	for (int i = 0; i < UNIX_SOCKET_MAX_PAIRS; i++)
	{
		/* 使用可能なsocket pairを探す */
		if (!unix_socket_pairs[i].in_use)
		{
			unix_socket_pairs[i].in_use = 1;
			unix_socket_pairs[i].owner_pid = current->pid;
			unix_socket_pairs[i].endpoints[0].length = 0;
			unix_socket_pairs[i].endpoints[0].reader = NULL;
			unix_socket_pairs[i].endpoints[1].length = 0;
			unix_socket_pairs[i].endpoints[1].reader = NULL;
			sv[0] = UNIX_SOCKET_FD_BASE + i * 2; /* 1つ目のfd */
			sv[1] = sv[0] + 1;					 /* 2つ目のfd */
			return 0;
		}
	}
	/* 使用可能なsocket pairがない場合 */
	return -EAGAIN;
}

/** fdが使用中のUnix socket endpointを表すか確認する
 * @param fd 確認対象fd
 * @return 使用中socket fdなら1，それ以外は0
 */
int unix_socket_is_fd(int fd)
{
	int pair_index = unix_socket_pair_index(fd);
	/* 無効なfdの場合 */
	if (pair_index < 0)
	{
		return 0;
	}

	/* 使用中のsocket pairか確認 */
	return unix_socket_pairs[pair_index].in_use;
}

/** socket pairを作成したowner PIDを返す
 * @param fd 確認対象socket fd
 * @return owner PID。無効なfdなら-1
 */
pid_t unix_socket_owner(int fd)
{
	int pair_index = unix_socket_pair_index(fd);

	/* 無効なfdまたは未使用のsocket pairの場合 */
	if (pair_index < 0 || !unix_socket_pairs[pair_index].in_use)
	{
		return -1;
	}

	return unix_socket_pairs[pair_index].owner_pid;
}

/** Unix socket endpointからデータを読み取る
 * @param fd 読み取り元endpoint fd
 * @param buf 読み取り先buffer
 * @param size 読み取り上限byte数
 * @return 読み取ったbyte数，負数=エラー
 * @note データがない場合はTASK_INTERRUPTIBLEで待機する
 */
long unix_socket_read(int fd, char *buf, unsigned int size)
{
	int pair_index = unix_socket_pair_index(fd);
	int endpoint_index = unix_socket_endpoint_index(fd);

	/* 無効なfdまたは未使用のsocket pairの場合 */
	if (pair_index < 0 || endpoint_index < 0 || !unix_socket_pairs[pair_index].in_use)
	{
		return -EBADF;
	}
	/* 無効なバッファの場合 */
	if (!buf)
	{
		return -EINVAL;
	}
	/* 読み取りサイズが0の場合 */
	if (size == 0)
	{
		return 0;
	}

	struct unix_socket_endpoint *endpoint =
		&unix_socket_pairs[pair_index].endpoints[endpoint_index];	/* 読み取り対象のendpoint */
	struct unix_socket_pair *pair = &unix_socket_pairs[pair_index]; /* 読み取り対象のpair */

	while (1)
	{
		/** EFLAGS
		 * @brief EFLAGSを保存・復元する理由は，
		 *        データの読み取り中に割り込みが発生すると，
		 *        データの整合性が崩れる可能性があるため
		 */
		unsigned long flags;
		local_irq_save(flags);

		/* owner終了によってpairが解放された場合は待機を終了する */
		if (!pair->in_use)
		{
			local_irq_restore(flags);
			return -EBADF;
		}

		/* データがある場合は読み取る */
		if (endpoint->length > 0)
		{
			unsigned int count = endpoint->length < size ? endpoint->length : size; /* 読み取りbyte数 */

			memcpy(buf, endpoint->buffer, count);

			/* endpoint->bufferの一部が読み取られていない場合，
			 * その残りデータを先頭に移動する */
			if (endpoint->length > count)
			{
				memmove(endpoint->buffer, endpoint->buffer + count, endpoint->length - count);
			}

			/* 読み取り後のバッファ長を更新する */
			endpoint->length -= count;

			/* 読み取り後のreaderをクリアする */
			if (endpoint->reader == current)
			{
				endpoint->reader = NULL;
			}

			local_irq_restore(flags);
			return (long)count;
		}

		/* シグナルが保留中の場合，returnで抜ける */
		if (signal_pending())
		{
			/* 読み取り後のreaderをクリアする */
			if (endpoint->reader == current)
			{
				endpoint->reader = NULL;
			}

			local_irq_restore(flags);
			return -EINTR; /* シグナルによる割り込み */
		}

		/* データがない場合はTASK_INTERRUPTIBLEで待機する */
		endpoint->reader = current;
		current->__state = TASK_INTERRUPTIBLE;
		local_irq_restore(flags);
		schedule();
	}
}

/** Unix socket endpointからpeerへデータを書き込む
 * @param fd 書き込み元endpoint fd
 * @param buf 書き込み元buffer
 * @param size 書き込み要求byte数
 * @return 書き込んだbyte数，負数=エラー
 * @note peer受信bufferの空き容量分だけ書き込む
 */
long unix_socket_write(int fd, const char *buf, unsigned int size)
{
	int pair_index = unix_socket_pair_index(fd);
	int endpoint_index = unix_socket_endpoint_index(fd);

	/* 無効なfdまたは未使用のsocket pairの場合 */
	if (pair_index < 0 || endpoint_index < 0 || !unix_socket_pairs[pair_index].in_use)
	{
		return -EBADF;
	}
	/* 無効なバッファの場合 */
	if (!buf)
	{
		return -EINVAL;
	}
	/* 読み取りサイズが0の場合 */
	if (size == 0)
	{
		return 0;
	}

	struct unix_socket_endpoint *peer =
		&unix_socket_pairs[pair_index].endpoints[endpoint_index ^ 1]; /* 書き込み対象のpeer endpoint */

	/** EFLAGS
	 * @brief EFLAGSを保存・復元する理由は，
	 *        データの書き込み中に割り込みが発生すると，
	 *        データの整合性が崩れる可能性があるため
	 */
	unsigned long flags;
	local_irq_save(flags);

	unsigned int space = UNIX_SOCKET_BUF_SIZE - peer->length; /* peerのバッファの空き容量 */
	unsigned int count = size < space ? size : space;		  /* 書き込みbyte数 */

	/* 書き込み可能な場合はデータをコピーする */
	if (count > 0)
	{
		memcpy(peer->buffer + peer->length, buf, count);
		peer->length += count;
	}

	/* 読み取り待機中のプロセスがいる場合は起床させる */
	if (peer->reader)
	{
		wake_up_process(peer->reader);
		peer->reader = NULL; /* 読み取り待機中のプロセスをクリアする */
	}

	local_irq_restore(flags);
	return (long)count;
}

/** socketpairシステムコール本体
 * @param domain AF_UNIXのみ対応
 * @param type SOCK_STREAMのみ対応
 * @param protocol 0のみ対応
 * @param sv 作成したfd pairの格納先
 * @return 0=成功，負数=エラー
 */
long sys_socketpair(int domain, int type, int protocol, int sv[2])
{
	return (long)unix_socket_pair(domain, type, protocol, sv);
}
