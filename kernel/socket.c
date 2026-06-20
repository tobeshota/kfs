#include <kfs/errno.h>
#include <kfs/sched.h>
#include <kfs/socket.h>

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

	/* fdをpair indexに変換 */
	return (fd - UNIX_SOCKET_FD_BASE) / 2;
}

/* Unix socket subsystemを初期化する */
void unix_socket_init(void)
{
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
			sv[0] = UNIX_SOCKET_FD_BASE + i * 2;	/* 1つ目のfd */
			sv[1] = sv[0] + 1;	/* 2つ目のfd */
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
