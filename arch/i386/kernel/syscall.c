#include <kfs/errno.h>
#include <kfs/printk.h>
#include <kfs/syscall.h>

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

/**　システムコールテーブル
 * システムコール番号からハンドラ関数へのマッピング
 */
typedef long (*syscall_fn_t)(long, long, long, long, long);

static syscall_fn_t sys_call_table[NR_syscalls] = {
	[0 ... NR_syscalls - 1] = sys_ni_syscall,
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
	return sys_call_table[nr](arg1, arg2, arg3, arg4, arg5);
}
