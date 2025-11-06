/* カーネルのエントリポイント (boot.Sの _start シンボル) */
ENTRY(_start)


/** Memory Map
 * ┌────────────────────┐ 0x000000
 * │ BIOS/Boot Loader   │
 * ├────────────────────┤ 0x200000 (2MiB) ← . = 2M
 * │ .text (4K align)   │ ← Multiboot + コード
 * ├────────────────────┤ +4K境界
 * │ .rodata (4K align) │ ← 定数データ
 * ├────────────────────┤ +4K境界
 * │ .data (4K align)   │ ← 初期化変数
 * ├────────────────────┤ +4K境界
 * │ .bss (4K align)    │ ← 初期値なし変数 + スタック
 * └────────────────────┘
 */
SECTIONS
{
    /* カーネル開始アドレス
     * .はロケーションカウンタの意
     * 2 MiB (0x200000) からロード開始(BIOS と GRUB が 0-1 MiB を使用するため)
     */
    . = 2M;

    /* .text セクション: コード(読み取り可能・実行可能)
     * BLOCK(4K): 次のセクションを 4 KiB 境界から開始
     * ALIGN(4K): このセクションを 4 KiB 境界に配置
     */
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)  /* Multiboot ヘッダー (先頭 8 KiB 以内に必要) */
        *(.text)       /* すべてのオブジェクトファイルの実行コード */
    }

    /* .rodata セクション: 定数(読み取り専用)
     * 定数文字列、const 変数などを配置
     * メモリ保護により書き込み不可に設定可能
     */
    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
    }

    /* .data セクション: 初期値ありデータ(読み書き可能)
     * 初期値を持つグローバル変数、静的変数を配置
     * 初期値はバイナリファイルに含まれる
     */
    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    /* .ctors セクション: コンストラクタ関数ポインタ配列
     * C++の静的オブジェクトやGCCの__attribute__((constructor))関数
     * カバレッジのための__gcov_init呼び出しもここに含まれる
     * 起動時に__CTOR_LIST__から__CTOR_END__まで順に実行する必要がある
     */
    .ctors BLOCK(4K) : ALIGN(4K)
    {
        __CTOR_LIST__ = .;
        KEEP(*(.ctors))
        KEEP(*(SORT_BY_INIT_PRIORITY(.ctors.*)))
        __CTOR_END__ = .;
    }

    /* .dtors セクション: デストラクタ関数ポインタ配列
     * 終了時に__DTOR_LIST__から__DTOR_END__まで順に実行する必要がある
     * カバレッジデータの書き出し(__gcov_exit)もここに含まれる
     */
    .dtors BLOCK(4K) : ALIGN(4K)
    {
        __DTOR_LIST__ = .;
        KEEP(*(.dtors))
        KEEP(*(SORT_BY_INIT_PRIORITY(.dtors.*)))
        __DTOR_END__ = .;
    }

    /* .bss セクション: 初期値なしデータ(読み書き可能)
     * 実行時に自動的にゼロクリアされる
     * バイナリファイルには含まれず、ロード時にメモリ確保
     */
    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)  /* 複数ファイルで宣言された共通シンボル */
        *(.bss)    /* 初期値なしグローバル変数、スタック領域 (boot.S) */
    }

    /* カーネルの終端アドレス（メモリ管理で使用） */
    _kernel_end = .;
}

