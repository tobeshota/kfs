#ifndef _KFS_PANIC_H
#define _KFS_PANIC_H

/** 汎用レジスタをクリアする
 * @details
 * ■汎用レジスタ[GPRs: General Purpose Registers]をクリアする理由
 * hlt後，レジスタに残るデータ(機密情報)が
 * 物理アクセスによるメモリダンプで漏洩することを防ぐため．
 *
 * @note EBP[Extended Base Pointer](スタックフレームの底部)はクリアするがclobber listに含めない．
 * ■EBPをclobber listに含めない理由
 * EBPをclobber listに含める(コンパイラにEBPを変更すると知らせる)と，
 * コンパイラはスタックフレームが崩壊し，呼び出し元の関数のアドレスが失われ，
 * panic() の呼び出し元の関数に戻れなくなると判断し，
 * コンパイルエラー error: bp cannot be used in 'asm' here を起こすため．
 *
 * @remarks この処理は実行後，呼び出し元の関数に戻らないことを前提とする．
 *          EBPをクリアするとスタックフレームが崩壊し，呼び出し元の関数のアドレスが失われるため，
 *          呼び出し元の関数に戻ろうとすると未定義動作を引き起こす．
 *          なお，clear_gp_registers()自体はinline展開される(呼び出され，呼び出し元に戻るということがない)ため，
 *          スタックフレームの崩壊による影響はない
 */
static inline void clear_gp_registers(void)
{
	__asm__ __volatile__("xorl %%eax, %%eax\n\t"
						 "xorl %%ebx, %%ebx\n\t"
						 "xorl %%ecx, %%ecx\n\t"
						 "xorl %%edx, %%edx\n\t"
						 "xorl %%esi, %%esi\n\t"
						 "xorl %%edi, %%edi\n\t"
						 "xorl %%ebp, %%ebp\n\t"
						 :
						 :
						 : "eax", "ebx", "ecx", "edx", "esi", "edi");
}

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
