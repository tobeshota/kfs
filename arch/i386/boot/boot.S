/*このカーネルをどう扱ってロードしてほしいかを伝える設定 #defineみたいなもの */
.set ALIGN,    1<<0             /* ページサイズ（通常4 KiB）に揃えて読み込んでほしい、と通知するフラグ */
.set MEMINFO,  1<<1             /* カーネルがブートローダに対して， mem_lower/mem_upper 情報を multiboot_info に詰めて渡すよう要求するフラグ */
.set FLAGS,    ALIGN | MEMINFO  /* 上記フラグをまとめたもの */
.set MAGIC,    0x1BADB002       /* 「マルチブートヘッダが正しく存在すること」をブートローダへ知らせるための固定値 */
.set CHECKSUM, -(MAGIC + FLAGS) /* 破損検出のための軽量チェック。 */

/** .multibootセクションにマルチブートヘッダを配置する
 * @note マルチブートヘッダは最初の8KiB内に存在し、4バイト単位で整列されなければならない
 */
.section .multiboot, "a" /* "a": ロード可能．GRUBがMultibootヘッダを見つけるためのもの */
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

/** 初期ページディレクトリとページテーブル（4KBアライメント）
 * カーネルが独自のページテーブルを設定する前のブート時に使用される
 * バイナリに含めるため.boot.dataセクション（書き込み可能データ、BSSではない）に配置する必要がある
 */
.section .boot.data, "aw" /* "aw": ロード可能，書き込み可能 */
.align PAGE_SIZE

.globl boot_page_directory
boot_page_directory:
.space PAGE_SIZE, 0  /* PAGE_SIZEバイト(4096バイト)ゼロで埋める */


.align PAGE_SIZE
boot_page_table_identity:  /* 0-4MBの恒等マッピング */
.space PAGE_SIZE, 0


.align PAGE_SIZE
boot_page_table_kernel:    /* カーネル用Higher halfマッピング（0xC0000000以降） */
.space PAGE_SIZE, 0

/** Multiboot情報構造体へのポインタを保存する
 * .boot.dataセクション（物理アドレス）に配置してアクセスを容易にする
 */
.section .boot.data, "aw"
.align 4
.globl multiboot_info_ptr
multiboot_info_ptr:
.long 0

/* ブートストラップコードセクション - ページング有効化前に物理アドレスで実行される */
.section .boot, "ax" /* "ax": ロード可能，実行可能 */
.global _start /* リンカに_startシンボルをエクスポートするよう指示 */
.extern gdt_init
.extern start_kernel

_start:
    /** 割り込みを無効化（IDT設定前に割り込みが来ないようにするため）
     * @note 割り込みの有効化は
     *       arch/i386/drivers/tty/keyboard.c: kfs_keyboard_init() で行われる
     */
    cli

    /** Multiboot情報構造体へのポインタをmultiboot_info_ptrに保存する
     * @brief ブートローダがEAXにマジックナンバー，
     *        EBXにmultiboot_info構造体へのポインタをセットしている．
     *        これをmultiboot_info_ptrに保存する．
     */
    mov %ebx, multiboot_info_ptr

    /* ページング有効化前に初期ページテーブルをセットアップ */
    call setup_paging

    /* CR3レジスタにページディレクトリの物理アドレスをセットする */
    mov $boot_page_directory, %eax
    mov %eax, %cr3

    /* ページングを有効化させる
     * @brief CR0レジスタの31ビット目(PGビット)を1にセットする
     * @note  CR0レジスタの31ビット目(PGビット)が1のとき，
     *        ページングを有効化しCR3レジスタを使用する．
     */
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

/** 初期ページテーブルをセットアップ
 * @note この関数は物理アドレスで実行される（ページング有効化前）ため，
 *       .bootセクションに配置する必要がある
 */
setup_paging:
    /** ページディレクトリの全1024エントリ分(4KiB)を0で初期化する
     * @brief 1024回，boot_page_directory の先頭アドレスに0を4バイト分書き込む．
     *        つまり，boot_page_directoryを先頭アドレスから
     *        4バイト×1024回==4096バイト(4KiB) までゼロ埋めする．
     * EDI: boot_page_directory の先頭アドレス
     * EAX: 0
     * ECX: 1024
     */
    mov $boot_page_directory, %edi
    mov $0, %eax
    mov $1024, %ecx
    rep stosl

    /* identityページテーブルの全1024エントリ分(4KiB)を0で初期化する */
    mov $boot_page_table_identity, %edi
    mov $0, %eax
    mov $1024, %ecx
    rep stosl

    /* kernelページテーブルの全1024エントリ分(4KiB)を0で初期化する */
    mov $boot_page_table_kernel, %edi
    mov $0, %eax
    mov $1024, %ecx
    rep stosl

    /** 最初の4MB（0x00000000 - 0x00400000）の恒等マッピングをセットアップ
     * Higher halfへの遷移中に必要
     */
    mov $0, %eax                          /* EAX = 0 (物理アドレス) */
    mov $boot_page_table_identity, %edi   /* EDI = ページテーブルの先頭アドレス */
    mov $1024, %ecx                       /* ECX = 1024 (ループカウンタ) */
1:
    mov %eax, %edx                        /* EDXに物理アドレスをセットする */
    or $(PAGE_PRESENT | PAGE_WRITE), %edx /* EDXにセットした物理アドレスが指すページをPresentかつRead/Writeに設定 */
    mov %edx, (%edi)                      /* EDXの値をEDIが指すメモリ(現在のページテーブルエントリ)に書き込む */
    add $PAGE_SIZE, %eax                  /* EAXを次のページの物理アドレスに更新する */
    add $4, %edi                          /* EDIを次のページテーブルエントリに更新する */
    loop 1b                               /* ECXをデクリメントし，0でなければ1にジャンプ */

    /* カーネル用Higher halfマッピングをセットアップ（0xC0000000 - 0xC0400000）
     * 物理0x00000000 - 0x00400000にマップ
     */
    mov $0, %eax                          /* EAX = 0 (物理アドレス) */
    mov $boot_page_table_kernel, %edi     /* EDI = ページテーブルの先頭アドレス */
    mov $1024, %ecx                       /* ECX = 1024 (ループカウンタ) */
2:
    mov %eax, %edx                        /* EDXに物理アドレスをセットする */
    or $(PAGE_PRESENT | PAGE_WRITE), %edx /* EDXにセットした物理アドレスが指すページをPresentかつRead/Writeに設定 */
    mov %edx, (%edi)                      /* EDXの値をEDIが指すメモリ(現在のページテーブルエントリ)に書き込む */
    add $PAGE_SIZE, %eax                  /* EAXを次のページの物理アドレスに更新する */
    add $4, %edi                          /* EDIを次のページテーブルエントリに更新する */
    loop 2b                               /* ECXをデクリメントし，0でなければ2にジャンプ */

    /** ページディレクトリにページテーブルをインストール
     * エントリ0: 恒等マッピング（仮想0x00000000-0x003FFFFF → 物理0x00000000-0x003FFFFF）
     * @note 1つのページディレクトリエントリは4MB(0x400000)を管理する
     *       仮想アドレス0x00000000 / 4MB = 0 → エントリ0
     */
    mov $boot_page_table_identity, %eax
    or $(PAGE_PRESENT | PAGE_WRITE), %eax
    mov $boot_page_directory, %edi
    mov %eax, (%edi)                    /* boot_page_directory[0] = boot_page_table_identity | Present | Write */

    /** エントリ768: Higher halfカーネルマッピング（仮想0xC0000000-0xC03FFFFF → 物理0x00000000-0x003FFFFF）
     * @note 1つのページディレクトリエントリは4MB(0x400000)を管理する
     *       仮想アドレス0xC0000000(3GB) / 4MB = 768 → エントリ768
     *       エントリ768は仮想アドレス空間の3GB～3GB+4MBを管理する
     */
    mov $boot_page_table_kernel, %eax
    or $(PAGE_PRESENT | PAGE_WRITE), %eax
    mov $boot_page_directory, %edi
    add $(768 * 4), %edi                /* ページディレクトリの768番目のエントリ（4バイト×768）にオフセット */
    mov %eax, (%edi)                    /* boot_page_directory[768] = boot_page_table_kernel | Present | Write */

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
    call start_kernel

    /* ここで戻ってきた場合は無限停止 */
    cli
1:  hlt
    jmp 1b
