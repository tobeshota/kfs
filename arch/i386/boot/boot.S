/*このカーネルをどう扱ってロードしてほしいかを伝える設定 #defineみたいなもの */
.set ALIGN,    1<<0             /* ページサイズ（通常4 KiB）に揃えて読み込んでほしい、と通知するフラグ */
.set MEMINFO,  1<<1             /* ブートローダからカーネルに mem_lower/mem_upper 情報を multiboot_info に詰めて渡すよう要求するフラグ */
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
stack_bottom:
.skip 16384 /* スタック領域が枯渇しないためのある程度の大きさの16KiB分の領域を確保 */
stack_top:

.section .text /* コードセクションの開始 */
.global _start /* リンカに_startシンボルをエクスポートするよう指示 */
.type _start, @function /* _startシンボルが関数であることをリンカに伝える */
_start:
    /* スタックを設定 */
    mov $stack_top, %esp

    /* 高レベルカーネルに入る */
    call start_kernel

    /* ここで戻ってきた場合は無限停止 */
    cli
1:  hlt
    jmp 1b

.size _start, . - _start /* _start関数のサイズを計算してリンカに伝える */
