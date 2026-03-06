/**
 * pid_test_helper.c — PID ユニットテスト専用ヘルパー
 *
 * kernel/pid.c が公開する pid_nr_free を直接操作する。
 */

extern int pid_nr_free;

/** pid_nr_free を直接設定し、設定前の値を返す
 * @param val 設定する値（0 で PID 枯渇状態をシミュレート）
 * @return 設定前の pid_nr_free
 * @note ユニットテスト外では絶対に呼ばないこと
 */
int pid_set_nr_free_for_test(int val)
{
	int saved = pid_nr_free;
	pid_nr_free = val;
	return saved;
}
