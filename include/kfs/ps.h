#ifndef _KFS_PS_H
#define _KFS_PS_H

#include <kfs/pid.h>
#include <kfs/stddef.h>

#define KFS_PS_TTY_LEN 16
#define KFS_PS_TIME_LEN 16
#define KFS_PS_STAT_LEN 8
#define KFS_PS_CMD_LEN 128

/* プロセス情報を表示用にまとめた構造 */
struct kfs_ps_entry
{
	pid_t pid;					/* プロセスID*/
	pid_t ppid;					/* 親プロセスID */
	char tty[KFS_PS_TTY_LEN];	/* 制御端末（未解決時は "-" を入れる） */
	char time[KFS_PS_TIME_LEN];	/* 実行時間表示用文字列（簡易フォーマット） */
	char stat[KFS_PS_STAT_LEN];	/* 状態文字列（例: 'R','S','D','Z' 等） */
	char cmd[KFS_PS_CMD_LEN];	/* コマンド名/引数の先頭（長い場合は切り詰め） */
};

#endif /* _KFS_PS_H */
