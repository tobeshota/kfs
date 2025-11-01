/*
 * ブートローダ関連シンボルのテスト用スタブ
 *
 * boot.SやLinker.ldで定義されるシンボルは、
 * 単体テスト環境では利用できないため、ここでダミー定義を提供する。
 */

#include <kfs/multiboot.h>

/** multiboot_info_ptr のスタブ
 * boot.Sで定義されるMultiboot情報構造体へのポインタ。
 * 単体テストでは実際のMultiboot情報は存在しないため、NULLで初期化。
 */
struct multiboot_info *multiboot_info_ptr = 0;

/** _kernel_end のスタブ
 * linker.ldで定義されるカーネル終端アドレス。
 * 単体テストでは意味を持たないが、シンボル解決のために提供。
 */
char _kernel_end[1] = {0};
