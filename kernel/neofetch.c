#include <asm-i386/page.h>
#include <kfs/console.h>
#include <kfs/printk.h>
#include <shell.h>
#include <video/vga.h>

/* KFSバージョン */
#define KFS_VERSION "4.0.0"

/* メモリ計算用の定数 */
#define BYTES_PER_MIB (1024 * 1024)
#define PAGES_TO_MIB(pages) (((pages) * PAGE_SIZE) / BYTES_PER_MIB)

/* mm/page_alloc.cからのメモリ統計情報 */
extern unsigned long total_pages;
extern unsigned long nr_free_pages;
extern unsigned long kernel_end_pfn;

/** neofetch風のシステム情報画面を表示する
 * @brief
 * - 左側にASCIIアートロゴを表示する
 * - 右側にシステム情報を表示する
 */
void print_neofetch(void)
{
	/* カラー定義 */
	const uint8_t sky = kfs_vga_make_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
	const uint8_t sun = kfs_vga_make_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
	const uint8_t rays = kfs_vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	const uint8_t header = kfs_vga_make_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
	const uint8_t label = kfs_vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	const uint8_t value = kfs_vga_make_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

	/* メモリ情報を計算（単位: MiB） */
	const unsigned long total_mem_mib = PAGES_TO_MIB(total_pages);
	const unsigned long free_mem_mib = PAGES_TO_MIB(nr_free_pages);
	const unsigned long used_mem_mib = total_mem_mib - free_mem_mib;
	const unsigned long kernel_mem_mib = PAGES_TO_MIB(kernel_end_pfn);

	printk("\n\n\n\n");

	/* 1行目: 星 + root@kfs */
	kfs_terminal_set_color(sky);
	printk("                  *                     ");
	kfs_terminal_set_color(header);
	printk("root@kfs\n");

	/* 2行目: 星 + セパレータ */
	kfs_terminal_set_color(sky);
	printk("                    *                   ");
	kfs_terminal_set_color(header);
	printk("---------\n");

	/* 3行目: 太陽の光線（上） + OS */
	kfs_terminal_set_color(rays);
	printk("           *     \\ | /                  ");
	kfs_terminal_set_color(label);
	printk("OS:       ");
	kfs_terminal_set_color(value);
	printk("kfs i386\n");

	/* 4行目: 光線 + カーネルバージョン */
	kfs_terminal_set_color(rays);
	printk("                  \\|/                   ");
	kfs_terminal_set_color(label);
	printk("Kernel:   ");
	kfs_terminal_set_color(value);
	printk("%s\n", KFS_VERSION);

	/* 5行目: 太陽と地平線 + メモリ */
	kfs_terminal_set_color(sun);
	printk("       - -- -- ---   ----- --- -        ");
	kfs_terminal_set_color(label);
	printk("Memory:   ");
	kfs_terminal_set_color(value);
	printk("%lu MiB / %lu MiB\n", used_mem_mib, total_mem_mib);

	/* 6行目: 光線 + 空きメモリ */
	kfs_terminal_set_color(rays);
	printk("                  /|\\                   ");
	kfs_terminal_set_color(label);
	printk("  Free:   ");
	kfs_terminal_set_color(value);
	printk("%lu MiB\n", free_mem_mib);

	/* 7行目: 光線（下） + カーネルメモリ */
	kfs_terminal_set_color(rays);
	printk("           *     / | \\     *            ");
	kfs_terminal_set_color(label);
	printk("  Kernel: ");
	kfs_terminal_set_color(value);
	printk("%lu MiB\n", kernel_mem_mib);

	/* 8行目: 星 + シェル */
	kfs_terminal_set_color(sky);
	printk("              *         *               ");
	kfs_terminal_set_color(label);
	printk("Shell:    ");
	kfs_terminal_set_color(value);
	printk("%s\n", SHELL_NAME);

	/* 9行目: 星 + ターミナル */
	kfs_terminal_set_color(sky);
	printk("                  *                     ");
	kfs_terminal_set_color(label);
	printk("Terminal: ");
	kfs_terminal_set_color(value);
	printk("VGA Text Mode 80x25\n");

	/* 空行 */
	printk("\n");

	/* カラーパレット */
	printk("                                        ");
	for (int i = 0; i < 8; i++)
	{
		uint8_t color = kfs_vga_make_color(VGA_COLOR_WHITE, i);
		kfs_terminal_set_color(color);
		printk("  ");
	}
	kfs_terminal_set_color(kfs_vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
	printk("\n\n\n\n\n\n\n\n\n\n");
}
