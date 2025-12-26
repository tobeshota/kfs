/** カーネルのエントリポイントを_startシンボルに指定する
 * @brief GRUBがカーネルをロードした後，
 *        ジャンプする関数の先頭アドレスを指定する
 * @note _startシンボルはboot.Sで定義されている
 */
ENTRY(_start)


/** Memory Map (Higher Half Kernel)
 * @details カーネルは物理アドレス0x00100000にロードされるが，
 *          仮想アドレス0xC0100000で動作することを示す
 *
 * Physical Address (LMA)      Virtual Address (VMA)
 * ┌────────────────────┐      ┌────────────────────┐
 * │ BIOS/Boot Loader   │      │                    │
 * ├────────────────────┤      │                    │
 * │ 0x200000 (2MiB)    │  →   │ 0xC0200000         │
 * │ .text (4K align)   │      │ Kernel Image       │
 * │ .rodata            │      │ (Higher Half)      │
 * │ .data              │      │                    │
 * │ .bss               │      │                    │
 * └────────────────────┘      └────────────────────┘
 *
 * セクション名     | Virtual Memory Address (VMA)      | Load Memory Address (LMA)     | アライメント   | PT_LOAD | 役割
 * ---------------+-----------------------------------+-------------------------------+--------------+---------+-------------------------------
 *  .multiboot    | 0x00100000                        | 0x00100000                    | 4 バイト      | boot    | Multiboot ヘッダ
 *                |                                   |                               |              |         | GRUB が先頭 8KiB を走査して検出
 *
 *  .boot         | 0x0010xxxx (4KiB Align)           | 0x0010xxxx                    | 4 KiB        | boot    | ブートストラップコード
 *                |                                   |                               |              |         | ページング有効前に物理アドレスで実行
 *
 *  .boot.data    | 0x0010yyyy (4KiB Align)           | 0x0010yyyy                    | 4 KiB        | boot    | ブート用データ
 *                |                                   |                               |              |         | ページテーブル等(書き込み可，BSS不可)
 *
 * ---------------+-----------------------------------+-------------------------------+--------------+---------+-------------------------------
 *  ここから Higher Half Kernel(高位仮想アドレス)
 * ---------------+-----------------------------------+-------------------------------+--------------+---------+-------------------------------
 *
 *  .text         | 0xC0000000 + boot終端 (4KiB Align) | VMA - 0xC0000000              | 4 KiB        | text    | カーネル実行コード
 *                |                                   |                               |              |         | ページング有効後に高位アドレスで実行
 *
 *  .rodata       | Higher Half                       | VMA - 0xC0000000              | 4 KiB        | rodata  | 読み取り専用データ
 *
 *  .data         | Higher Half                       | VMA - 0xC0000000              | 4 KiB        | data    | 初期化済み書き換え可能データ
 *
 *  .bss          | Higher Half                       | VMA - 0xC0000000              | 4 KiB        | data    | 未初期化データ(起動時に 0 クリア)
 *
 * ---------------+-----------------------------------+-------------------------------+--------------+---------+-------------------------------
 *
 *  関連シンボル:
 *    _start                   : カーネルのエントリポイント(boot.S で定義)
 *    __boot_end_phys           : ブート用セクションの物理終端アドレス
 *    __kernel_virtual_start    : Higher Half カーネル開始アドレス
 *    __kernel_virtual_end      : カーネル終端アドレス
 *
 *  補足:
 *  - .multiboot / .boot / .boot.data は物理＝仮想の identity mapping
 *  - Higher Half セクションでは AT() により VMA と LMA を分離
 *  - 実際にメモリへロードされる領域は PT_LOAD セグメントで決定される
 */

/** プログラムヘッダ
 * @brief ELFファイルに4つのロード可能セグメントを作成する
 */
PHDRS
{
    boot PT_LOAD;
    text PT_LOAD;
    rodata PT_LOAD;
    data PT_LOAD;
}

SECTIONS
{
    /** 現在の位置カウンタを 1 MiB (0x100000) に設定する
     * @brief 次のセクション(.)を物理アドレス 1 MiB (0x100000) から配置する．
     *        ここで . は現在の位置カウンタ(次に配置されるセクションやデータの開始アドレス)である．
     *        よって，次のセクションである .multiboot セクションは
     *        物理アドレス 1 MiB (0x100000) から配置される．
     * @note  Multibootヘッダは最初の8KiB以内に配置する必要がある (GRUBが検出するため)
     */
    . = 1M;

    /** .multibootセクション
     * @brief   出力セクション .multiboot を作り，
     *          現在の位置カウンタ . を 4バイト境界に揃えたアドレスから配置し，
     *          全入力オブジェクト(*.oファイル)中の.multiboot入力セクションをすべて集めて入れ，
     *          それをプログラムヘッダ boot (PT_LOADセグメント) に属させる
     *
     * @details
     * .multiboot 出力セクション名．最終バイナリ(ELF)に現れるセクション
     *
     * ALIGN(4)
     * 出力セクション .multiboot の 開始アドレスを4バイト境界に揃える．
     * 実態は . = ALIGN(4); を暗黙に実行し，アドレスが4で割り切れるようにする
     * 現在の .    ALIGN(4) 後の .
     * 0x100000   0x100000
     * 0x100001   0x100004
     * 0x100002   0x100004
     * 0x100003   0x100004
     *
     * *(.multiboot)
     * 全入力オブジェクト(*.oファイル)の.multiboot入力セクション(Multibootヘッダを含む)を集める
     *
     * KEEP(*(.multiboot))
     * 最適化防止フラグ．全入力オブジェクトの.multiboot入力セクションを
     * --gc-sections によるセクション削除の対象外にする
     */
    .multiboot ALIGN(4) :
    {
        KEEP(*(.multiboot))
    } :boot

    /** .bootセクション: ブートストラップコード(ページング有効前、物理アドレス)
     * @brief 出力セクション .boot を作り，
     *        現在の位置カウンタ . を 4KB境界に揃えたアドレスから配置し，
     *        全入力オブジェクト(*.oファイル)中の.boot入力セクションをすべて集めて入れ，
     *        それをプログラムヘッダ boot (PT_LOADセグメント) に属させる．
     * @note  .bootセクションにおける現在の位置カウンタ . は
     *        直前のセクション(.multibootセクション)の終端アドレスの次のアドレスである．
     */
    .boot ALIGN(4K) :
    {
        *(.boot)
    } :boot

    /* .boot.dataセクション: ページテーブル(BSS不可，書き込み可)
     * @brief 出力セクション .boot.data を作り，
     *        現在の位置カウンタ . を 4KB境界に揃えたアドレスから配置し，
     *        全入力オブジェクト(*.oファイル)の.boot.dataセクションをすべて集め入れ，
     *        それをプログラムヘッダ boot (PT_LOADセグメント) に属させる．
     */
    .boot.data ALIGN(4K) :
    {
        *(.boot.data)
    } :boot

    __boot_end_phys = .;

    /* Higher Halfアドレスに切り替え(3GB + ブート終端) */
    . = 0xC0000000 + ALIGN(__boot_end_phys, 4K);

    __kernel_virtual_start = .;

    /* .textセクション: 実行可能コード */
    .text ALIGN(4K) : AT(ADDR(.text) - 0xC0000000)
    {
        *(.text*)
    } :text

    /* .rodataセクション: 読み取り専用データ */
    .rodata ALIGN(4K) : AT(ADDR(.rodata) - 0xC0000000)
    {
        *(.rodata*)
    } :rodata

    /* .dataセクション: 初期化済みデータ */
    .data ALIGN(4K) : AT(ADDR(.data) - 0xC0000000)
    {
        *(.data*)
    } :data

    /* .bssセクション: 未初期化データ */
    .bss ALIGN(4K) : AT(ADDR(.bss) - 0xC0000000)
    {
        *(COMMON)
        *(.bss*)
    } :data

    /* カーネル終端アドレス */
    __kernel_virtual_end = .;
    _kernel_end = __kernel_virtual_end;
}

