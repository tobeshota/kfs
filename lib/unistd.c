#include <kfs/pid.h>	 /* pid_t */
#include <kfs/sched.h>	 /* uid_t */
#include <kfs/signal.h>	 /* sighandler_t */
#include <kfs/syscall.h> /* __NR_fork, __NR_exit, __NR_wait */

/**
 * POSIX プロセス管理 API — ring-3 から INT 0x80 で syscall を発行する実装
 *
 * 【ring-3 → ring-0 の瞬間】
 * 各関数内の `int $0x80` 命令を CPU が実行した瞬間に遷移が起きる。
 * CPU は IDT の system_gate (vector 0x80) を参照し、
 *   1. ring-3 の SS/ESP/EFLAGS/CS/EIP をカーネルスタックに自動 push
 *   2. CS を __KERNEL_CS（CPL=0）にセット
 *   3. EIP を system_call ハンドラ（entry.S）に変更
 * この一連の動作が `int $0x80` の1命令で完結し、次の命令は ring-0 で動く。
 *
 * 【ring-0 → ring-3 への帰還】
 * syscall 終了後、entry.S の RESTORE_ALL 末尾の `iret` が
 * カーネルスタックに積まれた CS（__USER_CS, CPL=3）を復元して ring-3 に戻る。
 * この関数群の `int $0x80` の「次の C 命令」は既に ring-3 で動いている。
 *
 * ring-0 から呼ぶと CPU が ss/esp を
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

/* 子プロセスの終了を待ち，終了した子プロセスを揮発させる */
pid_t wait(int *wstatus)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_wait), "b"(wstatus) : "memory");
	return (pid_t)ret;
}

/* fd にバイト列を書き込む（1=stdout, 2=stderr, 4=COM1シリアル） */
int write(int fd, const void *buf, unsigned int count)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_write), "b"(fd), "c"(buf), "d"(count) : "memory");
	return (int)ret;
}

/* 指定ミリ秒スリープする（HZ=1000 なので ms == tick 数） */
int msleep(unsigned int ms)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_msleep), "b"((long)ms) : "memory");
	return (int)ret;
}

/* 現在のプロセスの UID を返す */
uid_t getuid(void)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_getuid) : "memory");
	return (uid_t)ret;
}

/* 指定プロセスにシグナルを送る */
int kill(pid_t pid, int sig)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_kill), "b"(pid), "c"(sig) : "memory");
	return (int)ret;
}

/* シグナルハンドラを登録する */
sighandler_t signal(int sig, sighandler_t handler)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_signal), "b"(sig), "c"(handler) : "memory");
	return (sighandler_t)ret;
}

/* PSG チャンネルで音を鳴らす（ch=0,1,2: 矩形波, ch=3: ノイズ） */
int psg_note(int ch, unsigned int freq_hz, unsigned int deadline_ms)
{
	long ret;
	__asm__ __volatile__("int $0x80"
						 : "=a"(ret)
						 : "0"(__NR_psg_note), "b"((long)ch), "c"((long)freq_hz), "d"((long)deadline_ms)
						 : "memory");
	return (int)ret;
}

/* PSG チャンネルを停止する */
int psg_stop(int ch)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_psg_stop), "b"((long)ch) : "memory");
	return (int)ret;
}
