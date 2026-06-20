#ifndef _ASM_I386_SYSTEM_H
#define _ASM_I386_SYSTEM_H

/** 現在のEFLAGSを保存してローカルCPUの割り込みを禁止する
 * @param flags 呼び出し前のEFLAGSを格納するunsigned long変数
 * @details
 * pushfl  ; 現在の EFLAGS（中にIF[Interrupt Flag]がある）をスタックに積む
 * popl %0 ; スタックから EFLAGS を取り出し、変数 flags に格納する
 * cli     ; IF[Interrupt Flag] を 0 にして割り込みを禁止する
 * @note C言語風に書くと `flags = EFLAGS; EFLAGS.IF = 0;`
 */
#define local_irq_save(flags) __asm__ __volatile__("pushfl ; popl %0 ; cli" : "=g"(flags) : : "memory")

/** 保存済みEFLAGSを復元する
 * @param flags local_irq_save()が保存したEFLAGS
 * @details
 * pushl %0 ; 引数 flags をスタックに積む
 * popfl    ; スタックから EFLAGS を取り出し、CPU フラグを復元する
 * @note C言語風に書くと `EFLAGS = flags;`
 */
#define local_irq_restore(flags)                                                                                       \
	do                                                                                                                 \
	{                                                                                                                  \
		__asm__ __volatile__("pushl %0 ; popfl" : : "g"(flags) : "memory", "cc");                                      \
	} while (0)

#endif /* _ASM_I386_SYSTEM_H */
