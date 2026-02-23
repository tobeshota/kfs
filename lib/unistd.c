#include <kfs/pid.h>	 /* pid_t */
#include <kfs/syscall.h> /* __NR_fork, __NR_exit, __NR_wait */

/**
 * POSIX プロセス管理 API — ring-3 から INT 0x80 で syscall を発行する実装
 *
 * ring-3 から呼ばれることを前提とする。ring-0 から呼ぶと CPU が ss/esp を
 * スタックに積まないため pt_regs レイアウトが壊れる。
 * 単体テストからは run_in_ring3() で ring-3 に降りてから呼ぶこと。
 */

/* 現在のプロセスを複製する */
pid_t fork(void)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_fork) : "memory");
	return (pid_t)ret;
}

/* プロセスを終了する */
void __attribute__((noreturn)) exit(int status)
{
	__asm__ __volatile__("int $0x80" : : "a"(__NR_exit), "b"(status));
	__builtin_unreachable();
}

/* 子プロセスの終了を待つ */
pid_t wait(int *wstatus)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_wait), "b"(wstatus) : "memory");
	return (pid_t)ret;
}
