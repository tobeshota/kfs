#ifndef _SIGNAL_H_
#define _SIGNAL_H_

/** シグナル番号
 * @see https://dsa.cs.tsinghua.edu.cn/oj/static/unix_signal.html
 */
#define SIGHUP 1   /* 制御端末やプロセスのハングアップ */
#define SIGINT 2   /* キーボードからの割り込み（Ctrl+C） */
#define SIGQUIT 3  /* キーボードからの終了操作（Ctrl+\） */
#define SIGILL 4   /* 不正な命令 */
#define SIGTRAP 5  /* トレース/ブレークポイント */
#define SIGABRT 6  /* アボート */
#define SIGBUS 7   /* バスエラー */
#define SIGFPE 8   /* 浮動小数点例外 */
#define SIGKILL 9  /* プロセスの強制終了 */
#define SIGUSR1 10 /* ユーザ定義シグナル1 */
#define SIGSEGV 11 /* Segmentation Fault */
#define SIGUSR2 12 /* ユーザ定義シグナル2 */
#define SIGPIPE 13 /* 読み手のいないパイプへの書き込み */
#define SIGALRM 14 /* リアルタイムクロック */
#define SIGTERM 15 /* プロセスの終了 */

/* シグナル数の上限 */
#define _NSIG 32
#define NSIG _NSIG

/* デフォルトハンドラと無視ハンドラ */
#define SIG_DFL ((sighandler_t)0)	 /* デフォルト動作 */
#define SIG_IGN ((sighandler_t)1)	 /* シグナルを無視 */
#define SIG_ERR ((sighandler_t) - 1) /* エラー戻り値 */

/** シグナルハンドラの関数型
 * @param sig シグナル番号
 */
typedef void (*sighandler_t)(int sig);

/** シグナルアクション構造体
 * @brief シグナルごとのハンドラ設定を保持する
 */
struct sigaction
{
	sighandler_t sa_handler; /* シグナルハンドラ関数 */
	unsigned long sa_flags;	 /* シグナルハンドラの動作を変更するためのフラグ */
};

sighandler_t signal(int sig, sighandler_t handler);
int raise(int sig);
void do_signal(void);
int signal_pending(void);

#endif /* _SIGNAL_H_ */
