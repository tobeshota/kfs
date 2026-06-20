#ifndef _KFS_SOCKET_H
#define _KFS_SOCKET_H

#include <kfs/pid.h>

/*
 * アドレスファミリ
 */
#define AF_UNIX 1 /* local to host (pipes) */

/*
 * タイプ
 */
#define SOCK_STREAM 1 /* ストリーム型ソケット */

#define UNIX_SOCKET_MAX_PAIRS 8							 /* 最大ソケットペア数 */
#define UNIX_SOCKET_BUF_SIZE 512						 /* ソケットバッファサイズ */
#define UNIX_SOCKET_FD_BASE 300							 /* ソケットFDの開始番号 */
#define UNIX_SOCKET_FD_COUNT (UNIX_SOCKET_MAX_PAIRS * 2) /* ソケットFDの総数 */

void unix_socket_init(void);
void unix_socket_reset(void);
int unix_socket_pair(int domain, int type, int protocol, int sv[2]);
int unix_socket_is_fd(int fd);
pid_t unix_socket_owner(int fd);
long sys_socketpair(int domain, int type, int protocol, int sv[2]);

#endif /* _KFS_SOCKET_H */
