/*
 * test/unit/support/shell_command_stub.c
 *
 * シェル関数のテスト用スタブ
 *
 * 本番コードのshell_run()はweak symbolとして定義されており、
 * このファイルの実装でオーバーライドされる。
 * これにより、テスト時は無限ループを回避できる。
 */

#include <kfs/shell.h>

/* shell_run()のテスト用オーバーライド
 * 無限ループに入らず即座に戻る */
void shell_run(void)
{
	return;
}
