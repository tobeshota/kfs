#include <kfs/errno.h>
#include <kfs/printk.h>
#include <kfs/ps.h>
#include <kfs/unistd.h>

/** ps コマンド（ユーザランド）エントリ
 * @brief Phase1 の骨組み。`ps_snapshot` を呼んで1件だけ取得する。
 * @details 後続コミットで引数解析・複数件取得・整形表示を追加する予定。
 */
void cmd_ps(void)
{
	/* 最大取得件数（スタック上確保） */
	enum
	{
		MAX_PS = 64,
	};
	struct kfs_ps_entry entries[MAX_PS];

	long n = ps_snapshot(entries, MAX_PS);
	if (n < 0)
	{
		printk("ps: snapshot failed\n");
		return;
	}

	/* ヘッダ（無指定モード） */
	printk("PID   TTY      TIME  CMD\n");

	for (long i = 0; i < n; i++)
	{
		struct kfs_ps_entry *e = &entries[i];
		/* フォーマット: PID  TTY  TIME  CMD
		 * NOTE: kernel の vsnprintf は '-' フラグをサポートしないため
		 * 左寄せ指定（"%-7s"）は使えない。右寄せ幅指定に変更する。
		 */
		printk("%5d %7s %5s %s\n", (int)e->pid, e->tty, e->time, e->cmd);
	}
}
