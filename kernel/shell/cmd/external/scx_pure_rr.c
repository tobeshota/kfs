#include <kfs/shell.h>
#include <kfs/stdio.h>
#include <kfs/unistd.h>

/** 引数が空白だけか確認する
 * @param args コマンド引数
 * @return 1=空, 0=余分な引数あり
 */
static int scx_pure_rr_args_empty(const char *args)
{
	while (*args == ' ')
	{
		args++;
	}
	return *args == '\0';
}

/** pure_rr sched_ext schedulerを起動する
 * @param arg コマンド引数
 */
void cmd_scx_pure_rr(void *arg)
{
	const char *args = (const char *)arg;

	/* 引数が無効な場合は使用方法を表示する */
	if (!scx_pure_rr_args_empty(args))
	{
		printf("Usage: scx_pure_rr\n");
		return;
	}

	int ret = sched_ext_load("pure_rr");
	if (ret < 0)
	{
		printf("scx_pure_rr: failed to load pure_rr (%d)\n", ret);
		return;
	}

	struct sched_ext_status status;
	if (sched_ext_status(&status) < 0)
	{
		(void)sched_ext_unload();
		return;
	}
	pid_t owner_pid = status.owner_pid;

	/** 無限ループで待機する
	 * @brief 無限ループで待機する理由は，
	 *        sys_sched_ext_load() でsched_ext schedulerをロードするプロセス（== cmd_scx_pure_rr()
	 * を実行するプロセス）の寿命と， ロードされたsched_extスケジューラのbackendの寿命を結び付けるためである．
	 *
	 * @details sys_sched_ext_load() でsched_ext schedulerをロードするプロセス の寿命と
	 *          ロードされたsched_extスケジューラのbackendの寿命が結びつく理由:
	 *
	 * main.c の start_kernel() が sched_init() を呼び，これは sched_ext_class.init() すなわち ext_init() を呼び，
	 * これは register_exit_hook(sched_ext_owner_exit) を呼ぶ．
	 * - register_exit_hook() により，プロセスは do_exit() 時に
	 *   invoke_exit_hooks() を通じて sched_ext_owner_exit() を呼ぶようになる．
	 * - sched_ext_owner_exit() は，その呼び出し元プロセス が sys_sched_ext_load() 呼び出し元プロセスであるとき，
	 *   sched_ext_unregister() を呼んでsched_ext backend を解除する
	 *
	 * 以上より，sys_sched_ext_load() でsched_ext schedulerをロードするプロセス（== cmd_scx_pure_rr()
	 * を実行するプロセス）がdo_exit() で終了すると， sched_ext_owner_exit() が呼ばれ，sched_ext backend が解除される．
	 */
	while (1)
	{
		/* 終了シグナルはuser mode復帰前に処理される。
		 * 無視されるシグナルによる-EINTRではowner processを終了しない。 */
		(void)msleep(1000);
		if (sched_ext_status(&status) < 0 || !status.enabled || status.owner_pid != owner_pid)
		{
			return;
		}
	}
}
