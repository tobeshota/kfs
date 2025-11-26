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
	movl %esp, %eax		/* pt_regsへのポインタを第1引数に */
	call do_exception	/* Cのハンドラを呼び出し */
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

/* ========== CPU例外エントリポイント (0x00-0x13) ========== */

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
