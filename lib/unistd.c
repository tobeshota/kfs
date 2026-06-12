#include <kfs/keyboard.h>
#include <kfs/neofetch.h>
#include <kfs/pid.h>	 /* pid_t */
#include <kfs/ps.h>		 /* struct kfs_ps_entry */
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

/* fd からデータを読み取る */
int read(int fd, void *buf, unsigned int count)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_read), "b"(fd), "c"(buf), "d"(count) : "memory");
	return (int)ret;
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

/* 引数pidで指定されたプロセスのプロセスグループIDをgpidに設定する */
int setpgid(pid_t pid, pid_t pgid)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_setpgid), "b"(pid), "c"(pgid) : "memory");
	return (int)ret;
}

/* 引数pidで指定されたプロセスのプロセスグループIDを返す */
pid_t getpgid(pid_t pid)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_getpgid), "b"(pid) : "memory");
	return (pid_t)ret;
}

/* 呼び出しプロセスのプロセスグループIDを返す */
pid_t getpgrp(void)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_getpgrp) : "memory");
	return (pid_t)ret;
}

/* セッションを作成し，呼び出し元プロセスをセッションリーダーかつプロセスグループリーダーにする */
pid_t setsid(void)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_setsid) : "memory");
	return (pid_t)ret;
}

/* 現在の端末のフォアグラウンドプロセスグループIDを返す */
pid_t tcgetpgrp(int fd)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_tcgetpgrp), "b"((long)fd) : "memory");
	return (pid_t)ret;
}

/* 現在の端末のフォアグラウンドプロセスグループIDをpgrpに設定する */
int tcsetpgrp(int fd, pid_t pgrp)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_tcsetpgrp), "b"((long)fd), "c"((long)pgrp) : "memory");
	return (int)ret;
}

/* 呼び出しプロセスの所属仮想コンソール番号を返す（0始まり） */
int ttynr(void)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_ttynr) : "memory");
	return (int)ret;
}

/* fd にバイト列を書き込む（1=stdout, 2=stderr, 4=COM1シリアル） */
int write(int fd, const void *buf, unsigned int count)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_write), "b"(fd), "c"(buf), "d"(count) : "memory");
	return (int)ret;
}

/* プロセスの操作を行なう */
int prctl(int option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5)
{
	long ret;
	__asm__ __volatile__("int $0x80"
						 : "=a"(ret)
						 : "0"(__NR_prctl), "b"((long)option), "c"((long)arg2), "d"((long)arg3), "S"((long)arg4),
						   "D"((long)arg5)
						 : "memory");
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

/** ps のスナップショットを取得する
 * @param entries ユーザ提供の配列ポインタ（カーネルがこの領域に書き込む）
 * @param max_entries entries 配列の要素数
 * @return 取得したエントリ数（>=0）または負数のエラーコード
 * @note このプロトタイプは lib/unistd.c のラッパー経由で syscall を呼び出す。
 */
long ps_snapshot(struct kfs_ps_entry *entries, size_t max_entries)
{
	long ret;
	__asm__ __volatile__("int $0x80"
						 : "=a"(ret)
						 : "0"(__NR_ps_snapshot), "b"(entries), "c"((long)max_entries)
						 : "memory");
	return ret;
}

int neofetch_info(struct kfs_neofetch_info *info)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_neofetch_info), "b"((long)info) : "memory");
	return (int)ret;
}

/* システムを再起動する */
int reboot(void)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_reboot) : "memory");
	return (int)ret;
}

/* システムを停止する */
int halt(void)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_halt) : "memory");
	return (int)ret;
}

/* キーボードレイアウトを変更する */
int kbd_set_layout(kbd_layout_t layout)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_kbd_set_layout), "b"((long)layout) : "memory");
	return (int)ret;
}

int kbd_read_event(struct kfs_keyboard_raw_event *event)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_kbd_read_event), "b"((long)event) : "memory");
	return (int)ret;
}

int kbd_clear_events(void)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_kbd_clear_events) : "memory");
	return (int)ret;
}

int kbd_set_raw_mode(int enabled)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_kbd_set_raw_mode), "b"((long)enabled) : "memory");
	return (int)ret;
}

int openpty(int *master_fd, int *slave_fd)
{
	long ret;
	__asm__ __volatile__("int $0x80"
						 : "=a"(ret)
						 : "0"(__NR_openpty), "b"((long)master_fd), "c"((long)slave_fd)
						 : "memory");
	return (int)ret;
}

/* カーネルパニックを発生させる */
void __attribute__((noreturn)) trigger_panic(void)
{
	__asm__ __volatile__("int $0x80" : : "a"(__NR_panic) : "memory");
	__builtin_unreachable();
}

/* mmap2 システムコールを呼び出す（MAP_ANONYMOUS のみサポート） */
void *mmap(void *addr, unsigned long len, int prot, int flags, int fd, unsigned long pgoff)
{
	long ret;
	/* pgoff は渡さない．
	 * その理由は，i386 int $0x80 の第6引数は EBP だが GCC との衝突で
	 * 拘束不可かつ， MAP_ANONYMOUS では仕様上無視される値のため．
	 */
	__asm__ __volatile__("int $0x80"
						 : "=a"(ret)
						 : "0"(__NR_mmap2), "b"((long)addr), "c"((long)len), "d"((long)prot), "S"((long)flags),
						   "D"((long)fd)
						 : "memory");
	(void)pgoff;
	return (void *)ret;
}

/* munmap システムコールを呼び出す */
int munmap(void *addr, unsigned long len)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_munmap), "b"((long)addr), "c"((long)len) : "memory");
	return (int)ret;
}

pid_t waitpid(pid_t pid, int *wstatus, int options)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_waitpid), "b"(pid), "c"(wstatus), "d"(options) : "memory");
	return (pid_t)ret;
}

int ioctl(int fd, unsigned int cmd, unsigned long arg)
{
	long ret;
	__asm__ __volatile__("int $0x80" : "=a"(ret) : "0"(__NR_ioctl), "b"(fd), "c"(cmd), "d"(arg) : "memory");
	return (int)ret;
}
