#define __NR_sigreturn 119

/** ring3シグナルハンドラトランポリン
 * このシンボルは、kernel/signal.c:setup_sigframe() によって
 * frame->pretcode として参照される．
 * C のプロローグ/エピローグがあってはならない．そうでない場合，
 * sys_sigreturn() の実行時にユーザーの ESP が frame->sig を指さなくなる．
 * そのため、完全なアセンブリで実装する必要がある．
 */
.globl sigreturn
.type sigreturn, @function
sigreturn:
	/* return sys_sigreturn(); を実行する */
	movl $__NR_sigreturn, %eax
	int $0x80
1:
	/* ここに辿り着くことはないはず．
	 * もし辿り着いてしまった場合は、シグナルハンドラからの復帰に失敗していることになる．
	 * そのため，hltでCPUを停止させる． */
	hlt
	jmp 1b	/* 無限ループ */
.size sigreturn, .-sigreturn
