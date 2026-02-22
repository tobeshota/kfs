/** 割り込み/例外エントリポイント
 * CPUが割り込み/例外を検出した際に最初に実行されるコード
 * レジスタを保存し、Cのハンドラを呼び出す
 */

/* カーネルデータセグメントセレクタ */
#define __KERNEL_DS 0x10

/**
 * SAVE_ALL - 全レジスタをスタックに保存
 * struct pt_regsの構造と一致させる（push順の逆がメンバ順）
 */
.macro SAVE_ALL
	cld					/* 文字列操作の方向フラグをクリア */
	pushl %gs
	pushl %fs
	pushl %es
	pushl %ds
	pushl %eax
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %edx
	pushl %ecx
	pushl %ebx
	/* セグメントレジスタをカーネルデータセグメントに設定 */
	movl $__KERNEL_DS, %edx
	movl %edx, %ds
	movl %edx, %es
	movl %edx, %fs
	movl %edx, %gs
.endm

/**
 * RESTORE_ALL - 全レジスタをスタックから復元
 */
.macro RESTORE_ALL
	popl %ebx
	popl %ecx
	popl %edx
	popl %esi
	popl %edi
	popl %ebp
	popl %eax
	popl %ds
	popl %es
	popl %fs
	popl %gs
	addl $4, %esp		/* orig_eax (error_code) をスキップ */
	iret				/* 割り込みから復帰 */
.endm

/**
 * 共通例外ハンドラ
 * スタック上にpt_regsを構築し、do_exception()を呼び出す
 * 入力: スタックにerror_codeとtrap_noが積まれている
 */
common_exception:
	SAVE_ALL
	pushl %esp			/* pt_regsへのポインタを引数としてpush */
	call do_exception	/* Cのハンドラを呼び出し */
	addl $4, %esp		/* 引数をpopしてスタックを復元 */
	RESTORE_ALL

/**
 * エラーコードなしの例外用マクロ
 * CPUがerror_codeをpushしない例外用（0をダミーとしてpush）
 */
.macro EXCEPTION_NOERRCODE name, trapno
.globl \name
\name:
	pushl $0			/* ダミーのerror_code */
	pushl $\trapno		/* trap番号（orig_eaxの位置に） */
	jmp common_exception
.endm

/**
 * エラーコードありの例外用マクロ
 * CPUが既にerror_codeをpushしている例外用
 */
.macro EXCEPTION_ERRCODE name, trapno
.globl \name
\name:
	pushl $\trapno		/* trap番号（error_codeの後にpush） */
	/* Note: スタック上でerror_codeとtrap_noを入れ替える */
	xchgl %eax, 4(%esp)	/* eaxとerror_codeを交換 */
	xchgl %eax, (%esp)	/* eaxとtrap_noを交換 → error_codeがorig_eaxに */
	jmp common_exception
.endm

/* ========== 例外エントリポイント (0x00-0x13) ========== */

EXCEPTION_NOERRCODE divide_error, 0				/* 0x00: Division by Zero */
EXCEPTION_NOERRCODE debug, 1					/* 0x01: Debug */
EXCEPTION_NOERRCODE nmi, 2						/* 0x02: NMI */
EXCEPTION_NOERRCODE int3, 3						/* 0x03: Breakpoint */
EXCEPTION_NOERRCODE overflow, 4					/* 0x04: Overflow */
EXCEPTION_NOERRCODE bounds, 5					/* 0x05: BOUND Range Exceeded */
EXCEPTION_NOERRCODE invalid_op, 6				/* 0x06: Invalid Opcode */
EXCEPTION_NOERRCODE device_not_available, 7		/* 0x07: Device Not Available */
EXCEPTION_ERRCODE   double_fault, 8				/* 0x08: Double Fault (error code) */
EXCEPTION_NOERRCODE coprocessor_segment_overrun, 9	/* 0x09: Coprocessor Segment Overrun */
EXCEPTION_ERRCODE   invalid_TSS, 10				/* 0x0A: Invalid TSS (error code) */
EXCEPTION_ERRCODE   segment_not_present, 11		/* 0x0B: Segment Not Present (error code) */
EXCEPTION_ERRCODE   stack_segment, 12			/* 0x0C: Stack Fault (error code) */
EXCEPTION_ERRCODE   general_protection, 13		/* 0x0D: General Protection (error code) */
EXCEPTION_ERRCODE   page_fault, 14				/* 0x0E: Page Fault (error code) */
/* 0x0F: Reserved */
EXCEPTION_NOERRCODE coprocessor_error, 16		/* 0x10: x87 FPU Error */
EXCEPTION_ERRCODE   alignment_check, 17			/* 0x11: Alignment Check (error code) */
EXCEPTION_NOERRCODE machine_check, 18			/* 0x12: Machine Check */
EXCEPTION_NOERRCODE simd_coprocessor_error, 19	/* 0x13: SIMD Floating-Point */

/* ========== ハードウェア割り込みエントリポイント (0x20-0x2F) ========== */

/**
 * 共通IRQハンドラ
 * スタック上にpt_regsを構築し、do_IRQ()を呼び出す
 * 入力: スタックにIRQ番号がorig_eaxとして積まれている
 */
common_interrupt:
	SAVE_ALL
	pushl %esp			/* pt_regsへのポインタを引数としてpush */
	call do_IRQ			/* CのIRQハンドラを呼び出し */
	addl $4, %esp		/* 引数をpopしてスタックを復元 */
	RESTORE_ALL

/**　IRQエントリポイント用マクロ
 * @param name  エントリポイント名
 * @param irq   IRQ番号 (0-15)
 * @note  CPUはerror_codeをpushしないので、スタック構造を例外と同じにする
 *        orig_eaxの位置にIRQ番号を置く
 */
.macro IRQ_ENTRY name, irq
.globl \name
\name:
	pushl $\irq			/* IRQ番号をorig_eaxの位置に */
	jmp common_interrupt
.endm

/* IRQ0-15のエントリポイント */
IRQ_ENTRY irq0, 0		/* IRQ0: PIT (Programmable Interval Timer) */
IRQ_ENTRY irq1, 1		/* IRQ1: Keyboard */
IRQ_ENTRY irq2, 2		/* IRQ2: 8259A */
IRQ_ENTRY irq3, 3		/* IRQ3: COM2 */
IRQ_ENTRY irq4, 4		/* IRQ4: COM1 */
IRQ_ENTRY irq5, 5		/* IRQ5: LPT2 */
IRQ_ENTRY irq6, 6		/* IRQ6: Floppy */
IRQ_ENTRY irq7, 7		/* IRQ7: LPT1 / Spurious */
IRQ_ENTRY irq8, 8		/* IRQ8: RTC (Real-Time Clock) */
IRQ_ENTRY irq9, 9		/* IRQ9: ACPI */
IRQ_ENTRY irq10, 10		/* IRQ10: Available */
IRQ_ENTRY irq11, 11		/* IRQ11: Available */
IRQ_ENTRY irq12, 12		/* IRQ12: PS/2 Mouse */
IRQ_ENTRY irq13, 13		/* IRQ13: FPU */
IRQ_ENTRY irq14, 14		/* IRQ14: Primary ATA */
IRQ_ENTRY irq15, 15		/* IRQ15: Secondary ATA */

/* ========== システムコールエントリポイント (INT 0x80) ========== */

/** レジスタ規約
 * EAX: システムコール番号
 * EBX: 第1引数
 * ECX: 第2引数
 * EDX: 第3引数
 * ESI: 第4引数
 * EDI: 第5引数
 * EAX: 戻り値
 */
.globl system_call
system_call:
	pushl %eax			/* orig_eax: システムコール番号を保存 */
	SAVE_ALL
	/* do_syscall(nr, arg1, arg2, arg3, arg4, arg5) を呼び出す */
	/* pt_regs構造体: ebx(ESP+0), ecx(+4), edx(+8), esi(+12), edi(+16), ..., orig_eax(+44) */

	/** システムコール番号(orig_eaxの値)をEAXレジスタに複写する
	 * @brief
	 * pt_regs->orig_eaxをEAXレジスタに複写する
	 *
	 * @details
	 * orig_eax全体(pt_regs構造体のorig_eaxメンバの先頭アドレスから終端アドレスまでの4バイト)の値(システムコール番号)を
	 * EAXレジスタに複写する
	 * ┌----------┬--------------------------------------------------------------------┐
	 * | 44(%esp) | 【SRC】システムコール番号を格納するRAM上の先頭アドレス                     |
	 * |          | (pt_regs構造体のorig_eaxメンバの先頭アドレス)                           |
	 * |          | (ESPレジスタが指すアドレス(pt_regs構造体の先頭)から44バイトずらしたアドレス) |
	 * ├----------┼--------------------------------------------------------------------┤
	 * | %eax     | 【DEST】システムコール番号を格納するCPUレジスタ(EAXレジスタ)               |
	 * ├----------┼--------------------------------------------------------------------┤
	 * | movl     | SRCの先頭アドレスからの4バイト(orig_eax全体)をDEST(EAXレジスタ)に複写する  |
	 * └----------┴--------------------------------------------------------------------┘
	 *
	 * @note
	 * pt_regs構造体のレイアウト
	 * ```include/asm-i386/ptrace.h
	 * struct pt_regs {
	 *      uint32_t ebx;       // オフセット 0  (4バイト) <- ESPレジスタの指す先頭アドレス
	 *      uint32_t ecx;       // オフセット 4  (4バイト)
	 *      uint32_t edx;       // オフセット 8  (4バイト)
	 *      uint32_t esi;       // オフセット 12 (4バイト)
	 *      uint32_t edi;       // オフセット 16 (4バイト)
	 *      uint32_t ebp;       // オフセット 20 (4バイト)
	 *      uint32_t eax;       // オフセット 24 (4バイト)
	 *      uint32_t ds;        // オフセット 28 (4バイト)
	 *      uint32_t es;        // オフセット 32 (4バイト)
	 *      uint32_t fs;        // オフセット 36 (4バイト)
	 *      uint32_t gs;        // オフセット 40 (4バイト)
	 *      uint32_t orig_eax;  // オフセット 44 (4バイト) ← ESPレジスタの指す先頭アドレスから
	 *                          //                        44バイトずらしたアドレス
	 *      // その他のメンバ...
	 * };
	 * ```
	 */
	movl 44(%esp), %eax

	/** pt_regs->ediをdo_syscallの第5引数に設定する
	 * @brief pt_regs->edi(現在のESPレジスタが指すアドレス(pt_regs->ebx)から16バイト先の値)を
	 *        スタックに積む
	 * @note pt_regs->ediはdo_syscallの第5引数
	 *       (call命令の直前に積む6個の引数のうち1番目にpushされるため)
	 * @note pushlでESPレジスタが4バイト減少するため，次のオフセット計算に影響する
	 */
	pushl 16(%esp)
	/** pt_regs->esiをdo_syscallの第4引数に設定する
	 * @brief pt_regs->esi(現在のESPレジスタが指すアドレス(pt_regs->ebx - 4バイト)から16バイト先の値)を
	 *        スタックに積む
	 * @note pt_regs->esiはdo_syscallの第4引数
	 *       (call命令の直前に積む6個の引数のうち2番目にpushされるため)
	 * @note pushlでESPレジスタが4バイト減少するため，次のオフセット計算に影響する
	 */
	pushl 16(%esp)
	/** pt_regs->edxをdo_syscallの第3引数に設定する
	 * @brief pt_regs->edx(現在のESPレジスタが指すアドレス(pt_regs->ebx - 8バイト)から16バイト先の値)を
	 *        スタックに積む
	 * @note pt_regs->edxはdo_syscallの第3引数
	 *       (call命令の直前に積む6個の引数のうち3番目にpushされるため)
	 * @note pushlでESPレジスタが4バイト減少するため，次のオフセット計算に影響する
	 */
	pushl 16(%esp)
	pushl 16(%esp)		/* pt_regs->ecxをdo_syscallの第2引数に設定する */
	pushl 16(%esp)		/* pt_regs->ebxをdo_syscallの第1引数に設定する */
	pushl %eax			/* EAXレジスタの値(システムコール番号)を設定する */
	call do_syscall

	addl $24, %esp		/* 6引数分をpopする */
	movl %eax, 24(%esp)	/* システムコールの返り値をpt_regs->eaxに設定する */
	RESTORE_ALL

/* task_struct のオフセット定数 */
#define TASK_STACK 4
#define TASK_MM 12
#define TASK_THREAD_SP 412
#define THREAD_SIZE 4096
#define TSS_ESP0 4

/** コンテキストスイッチ
 * @brief prev の callee-saved レジスタをカーネルスタック上に保存（位置をprev->thread.spに記録）し、
 *        next->thread.sp が指すカーネルスタックに ESP を切り替えてレジスタを復元する。
 *        あわせて TSS の esp0 更新と switch_mm() によるアドレス空間切り替えも行う。
 *
 * C 関数シグネチャ: void __switch_to(struct task_struct *prev, struct task_struct *next)
 *   引数1 (prev): 4(%esp) - 切り替え元タスク（call 後）
 *   引数2 (next): 8(%esp) - 切り替え先タスク（call 後）
 *
 * @details 以下の処理を行う：
 *   1. callee-saved レジスタ (EBX, ESI, EDI, EBP) を現在のスタックに保存
 *   2. 現在の ESP を prev->thread.sp に保存（prev のカーネルスタック位置を記録）
 *   3. next->thread.sp から ESP を復元（next のカーネルスタックに切り替え）
 *   4. callee-saved レジスタを next のスタックから復元
 *   5. TSS の esp0 を next->stack + THREAD_SIZE に更新
 *   6. switch_mm(prev->mm, next->mm) を呼び出してページディレクトリ切り替え
 *
 * @note struct task_struct のメモリレイアウト（sched.h より、i386 32-bit）：
 *       offset 4: stack   - カーネルスタックへのポインタ
 *       offset 12: mm     - メモリディスクリプタ（struct mm_struct *）
 *       offset 668: thread - コンテキストスイッチ用レジスタ保存領域
 *           offset 668: thread.sp - 退避済みスタックポインタ
 *           offset 672: thread.ip - 退避済み命令ポインタ
 *
 * @note struct tss_struct のメモリレイアウト（desc.h より）：
 *       offset 0: back_link (4バイト)
 *       offset 4: esp0 (4バイト) - 割り込み時のカーネルスタック
 *
 * @note THREAD_SIZE = 4096 (sched.h で定義)
 */
.globl __switch_to
__switch_to:
	movl 4(%esp), %eax     /* %eax = prev */
	movl 8(%esp), %edx     /* %edx = next */

	/** prevのハードウェアコンテキストを物理RAMに退避する
	 * @brief 「callee-savedレジスタ」を「prevのカーネルスタック領域」に書き込む．
	 * @note prevのカーネルスタック領域は prev->stack から prev->stack + THREAD_SIZE まで
	 */
	pushl %ebp
	pushl %ebx
	pushl %esi
	pushl %edi
	movl %esp, TASK_THREAD_SP(%eax)  /* prev->thread.sp = 現在のESP（次回の復帰位置を記録） */

	/** コンテキストスイッチ: ESPをnext->thread.spが指す位置に切り替える
	 * @brief ESPはカーネスタックを指す．これを prev->thread.sp から next->thread.sp が指す位置に切り替える
	 *        これにより，次の pop/ret 命令が prev のカーネルスタックから next のカーネルスタックを参照するようになる．
	 * @note next->thread.sp は prev のカーネルスタックに保存したレジスタの位置を指す
	 * @details ESPを付け替えることで，
	 *          その後の pop/ret が参照するメモリが prev のものから next のものになり，
	 *          CPUの制御フローが丸ごと next 側に移る．
	 */
	movl TASK_THREAD_SP(%edx), %esp  /* ESP = next->thread.sp */

	/** prevのハードウェアコンテキストを物理RAMから復元する
	 * @brief 「nextのカーネルスタック領域内の値」を「callee-savedレジスタ」に書き込む
	 * @note nextのカーネルスタック領域は next->stack から next->stack + THREAD_SIZE まで
	 */
	popl %edi
	popl %esi
	popl %ebx
	popl %ebp

	/** ハードウェア状態更新: TSS.esp0 と CR3 を next に合わせる
	 * @brief 次の割り込み時のカーネルスタックとページテーブルを next のものにする）
	 */
	/* TSS.esp0にnextのカーネルスタックの高アドレス側を書き込む
	 * 理由: CPUは割り込み発生時にTSS.esp0をESPにロードしてカーネルスタックとして使用するため．
	 */
	movl TASK_STACK(%edx), %ecx       /* %ecx = next->stack */
	addl $THREAD_SIZE, %ecx           /* %ecx = next->stack + THREAD_SIZE（カーネルスタックの高アドレス側） */
	movl %ecx, init_tss+TSS_ESP0      /* TSS.esp0 = nextのカーネルスタックの高アドレス側 */
	/* CR3をnextのページディレクトリに更新する
	 * 理由: CR3を更新することで、CPUが次にページテーブルを参照する際にnextのアドレス空間を使用するようになるため．
	 */
	pushl TASK_MM(%edx)               /* next->mm を引数2としてpush */
	pushl TASK_MM(%eax)               /* prev->mm を引数1としてpush */
	call switch_mm                    /* CR3 を next->mm のページディレクトリに更新 */
	addl $8, %esp                     /* cdeclとして，呼び出し元が引数2つ（8バイト）を破棄 */

	ret


/** fork() で生成された子タスクの最初のエントリポイント
 * @brief 子タスクが最初に __switch_to() で復帰する際に実行される
 *
 * @details
 * Phase 7 の最小実装。カーネルスレッド間のコンテキストスイッチに対応。
 * Phase 9（プロセス実装）で以下を追加予定：
 *   - pt_regs からレジスタを復元（ユーザーモードへの復帰）
 *   - kernel_thread の場合の関数呼び出し
 *
 * @note 現在の実装では、カーネルスレッドが __switch_to() で復帰する際に
 *       schedule() を抜けて呼び出し元に戻る
 */
.globl ret_from_fork
ret_from_fork:
	test	%ebx, %ebx         /* ebx == 0 か判定（ebx & ebx を評価）しZFフラグ等を更新する */
	jz	.Lret_from_fork_done   /* ZFフラグが0なら関数呼び出しをスキップ */
	call	*%ebx              /* ebx が指す関数を呼び出す */

.Lret_from_fork_done:
	ret

