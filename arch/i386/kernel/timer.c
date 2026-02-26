#include <asm-i386/io.h>
#include <asm-i386/ptrace.h>
#include <kfs/irq.h>
#include <kfs/sched.h>
#include <kfs/timer.h>

/** 起動からの tick 数
 * @brief HZ=1000 より 1 tick = 1ms．
 */
volatile uint32_t jiffies = 0;

/** IRQ0 ハンドラ
 * @brief 毎ティック jiffies をインクリメントし scheduler_tick() を呼び出す
 */
static int timer_interrupt(int irq, struct pt_regs *regs)
{
	(void)irq;
	(void)regs;
	jiffies++;
	scheduler_tick();
	return IRQ_HANDLED;
}

/* i8254 PIT を初期化する */
void timer_init(void)
{
	/* 1ms ごとに割り込みが発生するカウント値 = 1193182 / 1000 = 1193 */
	unsigned int count = CLOCK_TICK_RATE / HZ;

	/** i8254 チャネル 0 をモード 2（レートジェネレータ）に設定する
	 * @brief コントロールワード 0x34 をポート 0x43 に書き込む．
	 *        これは，チャネル0をモード2（レートジェネレータ）で動作させるための設定である．
	 *        これを行うことで，1.193182 MHz クロックで毎サイクルcountをデクリメントし，
	 *        countが1になったサイクルでOUTピンを1クロック幅LOWパルスにして
	 *        8259A PICにIRQ0の発生を要求し，次のクロックでcountを初期値(1193)に戻し，
	 *        以降も同様にcountをデクリメントし続ける（自動リロード）．
	 *
	 * @details i8254 PITとIRQ0の関係
	 * i8254 PIT のチャネル0にはOUTピン(出力ピン)があり，
	 * これがIRQ0(物理線)を通じて8259A PICのIRQ0入力に接続されている．
	 * ■物理的な接続:
	 * i8254 PITのOUTピン(出力ピン) -- IRQ0 line(物理線) --> 8259A PIC -- INTA --> CPU
	 *
	 * ■モード2の動作：
	 * Clock:  _|‾|_|‾|_|‾|_|‾|_|‾|_
	 * count:   3  2  1  N  N-1...
	 * OUT:    ‾‾‾‾‾‾‾‾‾|_|‾‾‾‾‾‾
	 *                  ↑1クロック幅LOWパルス．これがIRQ0 lineを介して8259A PICへの0番の割り込み要求となる
	 *
	 * @details 【図解】周期的割り込み発生の流れ
	 * timer_init() -> i8254: コントロールワード(PIT_CH0_RATE_GEN)
	 *              -> i8254: カウント値(count = CLOCK_TICK_RATE / HZ = 1193)
	 *              -> i8254: カウントダウン開始
	 *                        count, count-1, ..., 2, 1
	 *              -> i8254: countが1のとき OUT ピンを LOW パルスにして IRQ0 を発生させ，
	 *                        次のクロックで count を初期値(1193)に戻す
	 *              -> 8259A: IRQ番号(0番)
	 *              -> CPU:   INTA
	 *              -> 8259A: Interrupt Vector番号(0x20)
	 *              -> CPU:   ISRアドレス(irq0)
	 *                        -> do_IRQ()
	 *                        -> timer_interrupt()
	 *                        -> scheduler_tick()
	 *              -> i8254: 再びカウントダウン開始（自動リロードにより繰り返す）
	 *
	 * @details 【詳説】周期的割り込み発生の流れ:
	 * 1. IRQ番号 と Interrupt Vector番号 と ISRアドレス を対応づける
	 *    (IRQ0番 と 0x20 と irq0 を対応づける)
	 *   a. main() -> init_8259A() より，
	 *      IRQ番号(0番) から Interrupt Vector番号(0x20) を
	 *      8259A PIC(Programmable interrupt controller) が引けるようになる
	 *   b. main() -> idt_init()
	 *             -> init_IRQ()
	 *             -> set_intr_gate(0x20, irq0) より，
	 *      Interrupt Vector番号(0x20) から ISRアドレス(irq0) を
	 *      CPU が引けるようになる
	 *   c. main() -> timer_init()
	 *             -> request_irq(0, timer_interrupt, "timer", NULL) より，
	 *      IRQ番号(0番) から ISRアドレス(timer_interrupt) を
	 *      CPU(irq_actions配列) が引けるようになる
	 * 2. i8254 にコントロールワードとカウント値を書き込む
	 *   a. outb(PIT_MODE_CMD_PORT, PIT_CH0_RATE_GEN) で
	 *      チャネル 0 をモード 2（レートジェネレータ）に設定する
	 *   b. outb(PIT_CHANNEL0_PORT, lobyte) / outb(PIT_CHANNEL0_PORT, hibyte) で
	 *      カウント値(1193)を書き込む
	 * 3. i8254 がカウントダウンして IRQ0 を発生させる
	 *   a. i8254 が 1.193182 MHz クロックで count を毎サイクルデクリメントする
	 *   b. count が 1 のとき i8254 が OUT ピンを 1 クロック幅の LOW パルスにして
	 *      IRQ0 ライン（電圧）を介して 8259A マスター PIC に割り込み要求を送信する
	 *   c. 次のクロックで i8254 はカウント値を自動リロードして再びカウントダウンを開始する
	 * 4. IRQ番号 から Interrupt Vector番号 を経て対応する ISR を呼び出す
	 *   a. 8259A が 受信した割り込み要求を CPU に送信する
	 *   b. CPU が 8259A からの割り込み要求を受け取り，これが有効化されているとき，
	 *      2回の INTA (Interrupt Acknowledge) サイクルを発行する
	 *   c. 8259A が
	 *     - 1回目のサイクルで INTA を受け取り，割り込みの開始を認識する
	 *     - 2回目のサイクルで IRQ番号(0番) から Interrupt Vector番号(0x20) を引き出し，
	 *       CPU に送信する
	 *   d. CPU が受信した Interrupt Vector番号(0x20) から ISRアドレス(irq0) を引き出し，呼び出す．
	 *      irq0 (entry.S) は do_IRQ() を呼び出し，
	 *      do_IRQ() は登録されたハンドラ timer_interrupt() を呼び出す
	 *   e. timer_interrupt() が scheduler_tick() を呼び出す
	 * 5. 3〜4 を HZ=1000 の周期（約 1ms ごと）で繰り返す
	 *
	 * @see MODE 2: RATE GENERATOR
	 *      at https://www.scs.stanford.edu/10wi-cs140/pintos/specs/8254.pdf
	 */
	outb(PIT_MODE_CMD_PORT, PIT_CH0_RATE_GEN);

	/* 割り込みを発生させるまでのカウントの値の下位8bitを
	   PITのチャネル0に書き込む（lobyte first） */
	outb(PIT_CHANNEL0_PORT, count & 0xFF);
	/* 割り込みを発生させるまでのカウントの値の上位8bitを
	   PITのチャネル0に書き込む（hibyte second） */
	outb(PIT_CHANNEL0_PORT, (count >> 8) & 0xFF);

	/* IRQ0 に timer_interrupt を登録する */
	request_irq(0, timer_interrupt, "timer", NULL);
}
