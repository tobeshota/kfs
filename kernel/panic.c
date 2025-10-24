#include <kfs/panic.h>
#include <kfs/printk.h>
#include <kfs/stdarg.h>

/* スタックトレース表示（arch/i386/kernel/stacktrace.c） */
extern void show_stack(unsigned long *esp);

/**
 * panic - カーネルパニック
 *
 * 致命的エラー発生時にエラーメッセージを表示し、
 * カーネルを停止する。この関数から戻ることはない。
 *
 * @fmt: フォーマット文字列
 * @...: 可変長引数
 */
void panic(const char *fmt, ...)
{
	static char buf[1024];
	va_list args;

	/* 割り込みを無効化（これ以降は割り込み不可） */
	__asm__ __volatile__("cli");

	/* パニックメッセージを整形 */
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	/* パニックメッセージを表示 */
	printk(KERN_EMERG "Kernel panic: %s\n", buf);

	/* スタックトレースを表示（デバッグ用） */
	printk(KERN_EMERG "---[ stack trace ]---\n");
	dump_stack();

	/* カーネル停止（無限ループ） */
	printk(KERN_EMERG "---[ end Kernel panic ]---\n");

	/* CPUを停止する */
	for (;;)
	{
		__asm__ __volatile__("hlt");
	}
}
