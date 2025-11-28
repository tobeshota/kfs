#include <asm-i386/desc.h>
#include <asm-i386/ptrace.h>
#include <kfs/irq.h>
#include <kfs/panic.h>
#include <kfs/printk.h>
#include <kfs/string.h>

/* ========== IDT管理 ========== */

/* IDTテーブル（256エントリ × 8バイト = 2KB）*/
struct idt_entry idt[IDT_ENTRIES];

/* IDTR用ポインタ構造体 */
static struct desc_ptr idt_ptr;

/* 例外ハンドラのエントリポイント（entry.Sで定義） */
extern void divide_error(void);
extern void debug(void);
extern void nmi(void);
extern void int3(void);
extern void overflow(void);
extern void bounds(void);
extern void invalid_op(void);
extern void device_not_available(void);
extern void double_fault(void);
extern void coprocessor_segment_overrun(void);
extern void invalid_TSS(void);
extern void segment_not_present(void);
extern void stack_segment(void);
extern void general_protection(void);
extern void page_fault(void);
extern void coprocessor_error(void);
extern void alignment_check(void);
extern void machine_check(void);
extern void simd_coprocessor_error(void);

#define set_intr_gate(n, addr) _set_gate(idt, n, addr, IDT_GATE_INTERRUPT)
#define set_system_gate(n, addr) _set_gate(idt, n, addr, IDT_GATE_USER)
#define set_trap_gate(n, addr) _set_gate(idt, n, addr, IDT_GATE_TRAP)

/* ========== 例外ハンドラ ========== */

/** 例外名テーブル */
static const char *exception_names[] = {
	"Division by Zero",			   /* 0x00 */
	"Debug",					   /* 0x01 */
	"NMI",						   /* 0x02 */
	"Breakpoint",				   /* 0x03 */
	"Overflow",					   /* 0x04 */
	"BOUND Range Exceeded",		   /* 0x05 */
	"Invalid Opcode",			   /* 0x06 */
	"Device Not Available",		   /* 0x07 */
	"Double Fault",				   /* 0x08 */
	"Coprocessor Segment Overrun", /* 0x09 */
	"Invalid TSS",				   /* 0x0A */
	"Segment Not Present",		   /* 0x0B */
	"Stack Fault",				   /* 0x0C */
	"General Protection Fault",	   /* 0x0D */
	"Page Fault",				   /* 0x0E */
	"Reserved",					   /* 0x0F */
	"x87 FPU Error",			   /* 0x10 */
	"Alignment Check",			   /* 0x11 */
	"Machine Check",			   /* 0x12 */
	"SIMD Floating-Point",		   /* 0x13 */
};

#define NUM_EXCEPTIONS (sizeof(exception_names) / sizeof(exception_names[0]))

/** レジスタ内容を表示する（Linux 2.6.11: show_regs()相当） */
void show_regs(struct pt_regs *regs)
{
	printk("EIP: %04x:[<%08lx>]\n", (unsigned)regs->cs & 0xffff, (unsigned long)regs->eip);
	printk("EFLAGS: %08lx\n", (unsigned long)regs->eflags);
	printk("EAX: %08lx EBX: %08lx ECX: %08lx EDX: %08lx\n", (unsigned long)regs->eax, (unsigned long)regs->ebx,
		   (unsigned long)regs->ecx, (unsigned long)regs->edx);
	printk("ESI: %08lx EDI: %08lx EBP: %08lx ESP: %08lx\n", (unsigned long)regs->esi, (unsigned long)regs->edi,
		   (unsigned long)regs->ebp, (unsigned long)regs->esp);
	printk("DS: %04x ES: %04x FS: %04x GS: %04x\n", (unsigned)regs->ds & 0xffff, (unsigned)regs->es & 0xffff,
		   (unsigned)regs->fs & 0xffff, (unsigned)regs->gs & 0xffff);
}

/** 例外番号から例外名を取得する */
static const char *get_exception_name(unsigned int trap_no)
{
	if (trap_no < NUM_EXCEPTIONS)
	{
		return exception_names[trap_no];
	}
	return "Unknown Exception";
}

/** 共通例外ハンドラ（entry.Sから呼び出される） */
void do_exception(struct pt_regs *regs)
{
	unsigned int trap_no = regs->orig_eax;
	const char *name = get_exception_name(trap_no);

	printk("Exception %d: %s\n", trap_no, name);
	show_regs(regs);

	/* Breakpoint(0x03)とOverflow(0x04)は継続可能 */
	if (trap_no != 3 && trap_no != 4)
	{
		panic("Fatal exception %d: %s", trap_no, name);
	}
}

/* ========== 初期化 ========== */

/** 例外ハンドラをIDTに登録する */
void trap_init(void)
{
	set_intr_gate(0, divide_error);				   /* 0x00: Division by Zero */
	set_intr_gate(1, debug);					   /* 0x01: Debug */
	set_intr_gate(2, nmi);						   /* 0x02: NMI */
	set_system_gate(3, int3);					   /* 0x03: Breakpoint (ユーザーから呼べる) */
	set_system_gate(4, overflow);				   /* 0x04: Overflow (INTO命令) */
	set_intr_gate(5, bounds);					   /* 0x05: BOUND Range Exceeded */
	set_intr_gate(6, invalid_op);				   /* 0x06: Invalid Opcode */
	set_intr_gate(7, device_not_available);		   /* 0x07: Device Not Available */
	set_intr_gate(8, double_fault);				   /* 0x08: Double Fault */
	set_intr_gate(9, coprocessor_segment_overrun); /* 0x09: Coprocessor Segment Overrun */
	set_intr_gate(10, invalid_TSS);				   /* 0x0A: Invalid TSS */
	set_intr_gate(11, segment_not_present);		   /* 0x0B: Segment Not Present */
	set_intr_gate(12, stack_segment);			   /* 0x0C: Stack Fault */
	set_intr_gate(13, general_protection);		   /* 0x0D: General Protection */
	set_intr_gate(14, page_fault);				   /* 0x0E: Page Fault */
	/* 0x0F: Reserved (Intel予約) */
	set_intr_gate(16, coprocessor_error);	   /* 0x10: x87 FPU Error */
	set_intr_gate(17, alignment_check);		   /* 0x11: Alignment Check */
	set_intr_gate(18, machine_check);		   /* 0x12: Machine Check */
	set_intr_gate(19, simd_coprocessor_error); /* 0x13: SIMD Floating-Point */
}

/** IDTを初期化しCPUに登録する
 * @note GDT初期化後、割り込み有効化前に呼び出す
 */
void idt_init(void)
{
	printk("Initializing IDT...\n");

	/* IDTテーブルをゼロクリア */
	memset(idt, 0, sizeof(idt));

	/* 例外ハンドラをIDTに登録する */
	trap_init();

	/* ハードウェア割り込みハンドラをIDTに登録する */
	init_IRQ();

	/* IDTRポインタを設定 */
	idt_ptr.size = sizeof(idt) - 1;		 /* サイズ - 1 */
	idt_ptr.address = (uint32_t)&idt[0]; /* IDTのベースアドレス */

	/* LIDT命令でIDTをCPUに登録 */
	__asm__ __volatile__("lidt %0" : : "m"(idt_ptr));

	printk("IDT loaded at 0x%08lx (%d entries)\n", (unsigned long)&idt[0], IDT_ENTRIES);
}

/** テスト用: IDTエントリを取得する */
struct idt_entry *idt_get_entry(int n)
{
	if (n < 0 || n >= IDT_ENTRIES)
	{
		return NULL;
	}
	return &idt[n];
}
