/*このカーネルをどう扱ってロードしてほしいかを伝える設定 #defineみたいなもの */
.set ALIGN,    1<<0             /* ページサイズ（通常4 KiB）に揃えて読み込んでほしい、と通知するフラグ */
.set MEMINFO,  1<<1             /* カーネルがブートローダに対して， mem_lower/mem_upper 情報を multiboot_info に詰めて渡すよう要求するフラグ */
.set FLAGS,    ALIGN | MEMINFO  /* 上記フラグをまとめたもの */
.set MAGIC,    0x1BADB002       /* 「マルチブートヘッダが正しく存在すること」をブートローダへ知らせるための固定値 */
.set CHECKSUM, -(MAGIC + FLAGS) /* 破損検出のための軽量チェック。 */

/* マルチブートヘッダは最初の8KiB内に存在し、4バイト単位で整列されなければならない */
.section .multiboot
.align 4 /* 同セクション内での変数(.multibootセクションの場合MAGIC, FLAGS, CHECKSUM)の値を4バイト整列する */
.long MAGIC /* マルチブートヘッダのマジックナンバーを配置 */
.long FLAGS /* フラグを配置 */
.long CHECKSUM /* チェックサムを配置 */

/* 16KiBのスタック領域を確保 */
.section .bss
.align 16 /* x86 の System V ABI ではスタックが 16バイト境界に揃っていることが前提になっているから */
.globl stack_bottom /* スタック領域の下端（低位アドレス） */
stack_bottom:
.skip 16384 /* スタック領域が枯渇しないためのある程度の大きさの16KiB分の領域を確保 */
.globl stack_top /* スタック領域の上端（高位アドレス） */
stack_top:

/* Multiboot情報構造体へのポインタを保存 */
.section .data
.align 4
.globl multiboot_info_ptr
multiboot_info_ptr:
.long 0

.section .text /* コードセクションの開始 */
.global _start /* リンカに_startシンボルをエクスポートするよう指示 */
.type _start, @function /* _startシンボルが関数であることをリンカに伝える */
.extern gdt_init
_start:
    /* Multiboot情報構造体へのポインタを保存（EBXレジスタ） */
    mov %ebx, (multiboot_info_ptr)

    /* スタックを設定 */
    mov $stack_top, %esp

    /* 物理アドレス0x00000800にGDTをビルドしてロードする */
    call gdt_init

    /* 高レベルカーネルに入る */
    call start_unit_test_kernel

    /* ここで戻ってきた場合は無限停止 */
    cli
1:  hlt
    jmp 1b

.size _start, . - _start /* _start関数のサイズを計算してリンカに伝える */
