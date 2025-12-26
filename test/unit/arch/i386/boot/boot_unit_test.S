/*このカーネルをどう扱ってロードしてほしいかを伝える設定 #defineみたいなもの */
.set ALIGN,    1<<0             /* ページサイズ（通常4 KiB）に揃えて読み込んでほしい、と通知するフラグ */
.set MEMINFO,  1<<1             /* カーネルがブートローダに対して， mem_lower/mem_upper 情報を multiboot_info に詰めて渡すよう要求するフラグ */
.set FLAGS,    ALIGN | MEMINFO  /* 上記フラグをまとめたもの */
.set MAGIC,    0x1BADB002       /* 「マルチブートヘッダが正しく存在すること」をブートローダへ知らせるための固定値 */
.set CHECKSUM, -(MAGIC + FLAGS) /* 破損検出のための軽量チェック。 */

/* マルチブートヘッダは最初の8KiB内に存在し、4バイト単位で整列されなければならない */
.section .multiboot, "a"
.align 4 /* 同セクション内での変数(.multibootセクションの場合MAGIC, FLAGS, CHECKSUM)の値を4バイト整列する */
.long MAGIC /* マルチブートヘッダのマジックナンバーを配置 */
.long FLAGS /* フラグを配置 */
.long CHECKSUM /* チェックサムを配置 */

/* ページング定数 */
.set PAGE_PRESENT,    0x001  /* Presentビット */
.set PAGE_WRITE,      0x002  /* Read/Writeビット */
.set PAGE_SIZE,       4096   /* 4KBページサイズ */
.set KERNEL_OFFSET,   0xC0000000  /* Higher halfオフセット（3GB） */

/* 16KiBのスタック領域を確保 */
.section .bss
.align 16 /* x86 の System V ABI ではスタックが 16バイト境界に揃っていることが前提になっているから */
.globl stack_bottom /* スタック領域の下端（低位アドレス） */
stack_bottom:
.skip 16384 /* スタック領域が枯渇しないためのある程度の大きさの16KiB分の領域を確保 */
.globl stack_top /* スタック領域の上端（高位アドレス） */
stack_top:

/* 初期ページディレクトリとページテーブル（4KBアライメント）
 * カーネルが独自のページテーブルを設定する前のブート時に使用される
 * バイナリに含めるため.boot.dataセクション（書き込み可能データ、BSSではない）に配置する必要がある
 */
.section .boot.data, "aw"
.align PAGE_SIZE
.globl boot_page_directory
boot_page_directory:
.space PAGE_SIZE, 0  /* 1024エントリ * 4バイト = 4096バイト、ゼロで埋める */

.align PAGE_SIZE
boot_page_table_identity:  /* 0-4MBの恒等マッピング */
.space PAGE_SIZE, 0

.align PAGE_SIZE
boot_page_table_kernel:    /* カーネル用Higher halfマッピング（0xC0000000以降） */
.space PAGE_SIZE, 0
/* Multiboot情報構造体へのポインタを保存
 * .boot.dataセクション（物理アドレス）に配置してアクセスを容易にする
 */
.section .boot.data, "aw"
.align 4
.globl multiboot_info_ptr
multiboot_info_ptr:
.long 0

/* ブートストラップコードセクション - ページング有効化前に物理アドレスで実行される */
.section .boot, "ax"
.global _start /* リンカに_startシンボルをエクスポートするよう指示 */
.extern gdt_init
.extern start_kernel

_start:
    /** 割り込みを無効化（IDT設定前に割り込みが来ないようにするため）
     * @note 割り込みの有効化は
     *       arch/i386/drivers/tty/keyboard.c: kfs_keyboard_init() で行われる
     */
    cli

    /* Multiboot情報構造体へのポインタを保存（EBXレジスタ）
     * multiboot_info_ptrは.boot.dataセクション（物理アドレス）にあるので直接アクセス可能
     */
    mov %ebx, multiboot_info_ptr

    /* ページング有効化前に初期ページテーブルをセットアップ */
    call setup_paging

    /* ページングを有効化 */
    mov $boot_page_directory, %eax
    mov %eax, %cr3

    /* CR0.PGビット（ビット31）を設定してページングを有効化 */
    mov %cr0, %eax
    or $0x80000000, %eax
    mov %eax, %cr0

    /* ページングが有効になった！
     * Higher half仮想アドレスへジャンプ
     * higher_half_entryは.textセクション内の0xC0105xxxにある
     */
    mov $higher_half_entry, %ecx
    jmp *%ecx
_start_end:

/* 初期ページテーブルをセットアップ
 * この関数は物理アドレスで実行される（ページング有効化前）
 * .bootセクションに配置する必要がある
 */
setup_paging:
    /* ページディレクトリをクリア */
    mov $boot_page_directory, %edi
    mov $0, %eax
    mov $1024, %ecx
    rep stosl

    /* ページテーブルをクリア */
    mov $boot_page_table_identity, %edi
    mov $0, %eax
    mov $1024, %ecx
    rep stosl

    mov $boot_page_table_kernel, %edi
    mov $0, %eax
    mov $1024, %ecx
    rep stosl

    /* 最初の4MB（0x00000000 - 0x00400000）の恒等マッピングをセットアップ
     * Higher halfへの遷移中に必要
     */
    mov $0, %eax  /* 物理アドレス0から開始 */
    mov $boot_page_table_identity, %edi
    mov $1024, %ecx  /* 1024エントリ */

1:
    mov %eax, %edx
    or $(PAGE_PRESENT | PAGE_WRITE), %edx
    mov %edx, (%edi)
    add $PAGE_SIZE, %eax
    add $4, %edi
    loop 1b

    /* カーネル用Higher halfマッピングをセットアップ（0xC0000000 - 0xC0400000）
     * 物理0x00000000 - 0x00400000にマップ
     */
    mov $0, %eax  /* 物理アドレス0から開始 */
    mov $boot_page_table_kernel, %edi
    mov $1024, %ecx  /* 1024エントリ */

2:
    mov %eax, %edx
    or $(PAGE_PRESENT | PAGE_WRITE), %edx
    mov %edx, (%edi)
    add $PAGE_SIZE, %eax
    add $4, %edi
    loop 2b

    /* ページディレクトリにページテーブルをインストール
     * エントリ0: 恒等マッピング（0x00000000 - 0x00400000）
     */
    mov $boot_page_table_identity, %eax
    or $(PAGE_PRESENT | PAGE_WRITE), %eax
    mov $boot_page_directory, %edi
    mov %eax, (%edi)

    /* エントリ768（0xC0000000 / 4MB）: Higher halfカーネルマッピング */
    mov $boot_page_table_kernel, %eax
    or $(PAGE_PRESENT | PAGE_WRITE), %eax
    mov $boot_page_directory, %edi
    add $(768 * 4), %edi  /* 768番目のエントリ */
    mov %eax, (%edi)

    ret

/* Higher halfコード - 仮想0xC0105000以降の.textセクションに配置 */
.section .text
.global higher_half_entry
higher_half_entry:
    /* Higher halfアドレスでスタックをセットアップ */
    mov $stack_top, %esp

    /* 物理アドレス0x00000800にGDTをビルドしてロードする */
    call gdt_init

    /* 高レベルカーネルに入る */
    call start_unit_test_kernel

    /* ここで戻ってきた場合は無限停止 */
    cli
1:  hlt
    jmp 1b
