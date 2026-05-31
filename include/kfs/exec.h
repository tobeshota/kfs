#ifndef _KFS_EXEC_H
#define _KFS_EXEC_H

/** 指定した関数をユーザ空間のプロセスとして実行する
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
 *     do_wait(NULL, 0);
 * }
 */
#define exec_fn(fn, arg) __exec_fn(#fn, (void (*)(void *))(fn), arg)

void __attribute__((noreturn)) __exec_fn(const char *fn_name, void (*fn)(void *), void *arg);

#endif /* _KFS_EXEC_H */
