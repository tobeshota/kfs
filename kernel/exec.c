#include <kfs/sched.h>

extern void sys_exit(int status);

/** カーネル内の関数をプロセスとして実行する
 * @brief fn(arg) を実行して終了する（noreturn）
 * @param fn  実行する関数
 * @param arg fn に渡す引数．引数なしの場合は NULL を渡すこと
 * @note execve() と同様に呼び出し元には戻らない．
 *       fork() して子プロセスで呼ぶことが想定される．
 * @example
 * pid_t pid = do_fork();
 * if (pid == 0)
 * {
 *     exec_fn(my_function, my_arg);
 * }
 * else
 * {
 *     do_wait(NULL);
 * }
 */
void __attribute__((noreturn)) exec_fn(void (*fn)(void *), void *arg)
{
	fn(arg);
	sys_exit(0);
	__builtin_unreachable();
}
