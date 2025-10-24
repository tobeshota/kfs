/*
 * シェル関数のテスト用スタブ
 */
#include <kfs/shell.h>

/** shell_run()のテスト用オーバーライド
 * 本番コードのshell_run()はweak symbolとして定義されており、
 * このファイルの実装でオーバーライドされる。
 * これにより、テスト時は無限ループを回避できる。
 */
void shell_run(void)
{
	return;
}

/** dump_stack()のテスト用オーバーライド．
 * アーキテクチャ固有関数は単体テストで計測できないためオーバーライドする．
 * 単体テストでは実際のスタックダンプを行うことはない
 */
void dump_stack(void)
{
	return;
}
