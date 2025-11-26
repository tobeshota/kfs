/**
 * ptrace.h - レジスタ構造体定義
 *
 * 割り込み/例外発生時のCPUレジスタ保存用構造体
 * @see Linux 2.6.11: include/asm-i386/ptrace.h
 */
#ifndef _ASM_I386_PTRACE_H
#define _ASM_I386_PTRACE_H

#include <kfs/stdint.h>

/** 全レジスタ保存構造体
 * @note entry.SのSAVE_ALLマクロでスタックに積まれる順序と一致させる
 *       スタックは高アドレス→低アドレスに成長するため，
 *       構造体の先頭が最後にpushされたレジスタ（低アドレス）になる
 */
struct pt_regs
{
	/* SAVE_ALLでpushされるレジスタ（push順の逆=pop順） */
	uint32_t ebx; /* 汎用レジスタ */
	uint32_t ecx;
	uint32_t edx;
	uint32_t esi;
	uint32_t edi;
	uint32_t ebp;
	uint32_t eax;
	uint32_t ds; /* セグメントレジスタ（16ビットだが32ビットでpush） */
	uint32_t es;
	uint32_t fs;
	uint32_t gs;

	/* 割り込み/例外発生時にCPUが自動的にpushする値 */
	uint32_t orig_eax; /* システムコール番号 or エラーコード */
	uint32_t eip;	   /* 割り込み発生時の命令ポインタ */
	uint32_t cs;	   /* コードセグメント */
	uint32_t eflags;   /* フラグレジスタ */
	uint32_t esp;	   /* スタックポインタ（特権レベル変更時のみ有効） */
	uint32_t ss;	   /* スタックセグメント（特権レベル変更時のみ有効） */
};

/* レジスタ表示関数 */
void show_regs(struct pt_regs *regs);

#endif /* _ASM_I386_PTRACE_H */
