#ifndef _KFS_PANIC_H
#define _KFS_PANIC_H

/**
 * panic - カーネルパニック
 *
 * 致命的エラー発生時にカーネルを停止する。
 * この関数から戻ることはない。
 *
 * @fmt: フォーマット文字列
 * @...: 可変長引数
 */
void panic(const char *fmt, ...) __attribute__((format(printf, 1, 2))) __attribute__((noreturn));

#endif /* _KFS_PANIC_H */
