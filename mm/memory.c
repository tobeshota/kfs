/** 仮想アドレス空間の管理を行う
 * - 仮想メモリ領域（VMA）の作成・削除・検索
 * - カーネル空間とユーザ空間の管理
 */

#include <asm-i386/page.h>
#include <asm-i386/pgtable.h>
#include <kfs/gfp.h>
#include <kfs/mm.h>
#include <kfs/mm_types.h>
#include <kfs/printk.h>
#include <kfs/slab.h>
#include <kfs/stddef.h>
#include <kfs/string.h>

/* vmalloc()などのカーネル専用VMAはプロセスのmmとは別に管理する */
static struct vm_area_struct *kernel_vm_area_list = NULL;

/* カーネル仮想メモリの開始位置（ページング後の高位メモリ） */
/* Linux 2.6.11では VMALLOC_START に相当 */
#define KERNEL_VM_START 0xD0000000 /* 3.25GB */
#define KERNEL_VM_END 0xFFFFFFFF   /* 4GB */

/* ユーザ仮想メモリの範囲（0〜3GB のユーザ空間内） */
#define USER_VM_START 0x40000000UL /* 1GB: ユーザ mmap 開始 */
#define USER_VM_END 0xBFFF0000UL   /* ~3GB: ユーザ空間終端 */

/* 次に割り当て可能なカーネル仮想アドレス */
static unsigned long next_vm_addr = KERNEL_VM_START;

/** 指定アドレスを含むVMAをリストから検索する
 * @param list 検索対象VMAリストの先頭
 * @param addr 検索する仮想アドレス
 * @return 見つかったVMA，見つからない場合はNULL
 */
static struct vm_area_struct *find_vma_in_list(struct vm_area_struct *list, unsigned long addr)
{
	struct vm_area_struct *vma;

	for (vma = list; vma != NULL; vma = vma->vm_next)
	{
		if (addr >= vma->vm_start && addr < vma->vm_end)
		{
			return vma;
		}
	}
	return NULL;
}

/** VMAをリストへアドレス昇順で挿入する
 * @param list 挿入対象VMAリストの先頭ポインタ
 * @param new_vma 挿入するVMA
 * @return 成功時0，失敗時-1
 */
static int insert_vma_in_list(struct vm_area_struct **list, struct vm_area_struct *new_vma)
{
	struct vm_area_struct *vma;
	struct vm_area_struct *prev;

	if (list == NULL || new_vma == NULL)
	{
		return -1;
	}

	/* VMAの重なりはmunmapやfork時の所有範囲を曖昧にするため拒否する */
	for (vma = *list; vma != NULL; vma = vma->vm_next)
	{
		if (new_vma->vm_start < vma->vm_end && new_vma->vm_end > vma->vm_start)
		{
			printk(KERN_WARNING "insert_vm_area: overlap detected\n");
			return -1;
		}
	}

	if (*list == NULL || new_vma->vm_start < (*list)->vm_start)
	{
		new_vma->vm_next = *list;
		*list = new_vma;
		return 0;
	}

	prev = *list;
	for (vma = (*list)->vm_next; vma != NULL; vma = vma->vm_next)
	{
		if (new_vma->vm_start < vma->vm_start)
		{
			prev->vm_next = new_vma;
			new_vma->vm_next = vma;
			return 0;
		}
		prev = vma;
	}

	prev->vm_next = new_vma;
	new_vma->vm_next = NULL;
	return 0;
}

/** 指定開始アドレスのVMAをリストから取り外す
 * @param list 削除対象VMAリストの先頭ポインタ
 * @param addr 取り外すVMAの開始アドレス
 * @return 取り外したVMA，見つからない場合はNULL
 */
static struct vm_area_struct *remove_vma_from_list(struct vm_area_struct **list, unsigned long addr)
{
	struct vm_area_struct *vma;
	struct vm_area_struct *prev;

	if (list == NULL || *list == NULL)
	{
		return NULL;
	}

	if ((*list)->vm_start == addr)
	{
		vma = *list;
		*list = vma->vm_next;
		vma->vm_next = NULL;
		return vma;
	}

	prev = *list;
	for (vma = (*list)->vm_next; vma != NULL; vma = vma->vm_next)
	{
		if (vma->vm_start == addr)
		{
			prev->vm_next = vma->vm_next;
			vma->vm_next = NULL;
			return vma;
		}
		prev = vma;
	}
	return NULL;
}

/** 指定したアドレスを含むプロセスVMAを検索する
 * @param mm 検索対象のメモリディスクリプタ
 * @param addr 検索する仮想アドレス
 * @return 見つかったVMA，見つからない場合はNULL
 */
struct vm_area_struct *find_vma(struct mm_struct *mm, unsigned long addr)
{
	if (mm == NULL)
	{
		return NULL;
	}
	return find_vma_in_list(mm->mmap, addr);
}

/** プロセスVMAをmm_structのリストへ挿入する
 * @param mm 挿入先のメモリディスクリプタ
 * @param vma 挿入するVMA
 * @return 成功時0，失敗時-1
 */
int insert_vm_area(struct mm_struct *mm, struct vm_area_struct *vma)
{
	if (mm == NULL)
	{
		return -1;
	}
	return insert_vma_in_list(&mm->mmap, vma);
}

/** プロセスVMAをmm_structのリストから取り外す
 * @param mm 削除対象のメモリディスクリプタ
 * @param addr 取り外すVMAの開始アドレス
 * @return 取り外したVMA，見つからない場合はNULL
 */
struct vm_area_struct *remove_vm_area(struct mm_struct *mm, unsigned long addr)
{
	if (mm == NULL)
	{
		return NULL;
	}
	return remove_vma_from_list(&mm->mmap, addr);
}

/** カーネルVMAを検索する
 * @param addr 検索する仮想アドレス
 * @return 見つかったVMA，見つからない場合はNULL
 */
struct vm_area_struct *find_kernel_vma(unsigned long addr)
{
	return find_vma_in_list(kernel_vm_area_list, addr);
}

/** カーネルVMAを挿入する
 * @param vma 挿入するVMA
 * @return 成功時0，失敗時-1
 */
int insert_kernel_vm_area(struct vm_area_struct *vma)
{
	return insert_vma_in_list(&kernel_vm_area_list, vma);
}

/** カーネルVMAを取り外す
 * @param addr 取り外すVMAの開始アドレス
 * @return 取り外したVMA，見つからない場合はNULL
 */
struct vm_area_struct *remove_kernel_vm_area(unsigned long addr)
{
	return remove_vma_from_list(&kernel_vm_area_list, addr);
}

/** カーネル空間の未使用仮想アドレス領域を探す
 * @param len 必要なサイズ（バイト単位）
 * @return 使用可能な仮想アドレス，見つからない場合は0
 */
unsigned long get_unmapped_area(size_t len)
{
	struct vm_area_struct *vma;
	unsigned long addr;

	len = (len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	if (len == 0)
	{
		return 0;
	}

	if (kernel_vm_area_list == NULL)
	{
		if (next_vm_addr + len <= KERNEL_VM_END)
		{
			addr = next_vm_addr;
			next_vm_addr += len;
			return addr;
		}
		return 0;
	}

	addr = KERNEL_VM_START;
	for (vma = kernel_vm_area_list; vma != NULL; vma = vma->vm_next)
	{
		if (vma->vm_start >= addr + len)
		{
			return addr;
		}
		addr = vma->vm_end;
	}

	if (addr + len <= KERNEL_VM_END)
	{
		return addr;
	}
	return 0;
}

/** ユーザ空間の未使用仮想アドレス領域を探す
 * @param mm 検索対象のメモリディスクリプタ
 * @param len 必要なサイズ（バイト単位）
 * @return 使用可能なユーザ仮想アドレス，見つからない場合は0
 */
unsigned long get_unmapped_area_user(struct mm_struct *mm, size_t len)
{
	struct vm_area_struct *vma;
	unsigned long addr;

	if (mm == NULL)
	{
		return 0;
	}

	len = (len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	if (len == 0 || len > USER_VM_END - USER_VM_START)
	{
		return 0;
	}

	/* mmごとのVMAリストを走査するため，同じVAを別プロセスで再利用できる */
	addr = USER_VM_START;
	for (vma = mm->mmap; vma != NULL; vma = vma->vm_next)
	{
		if (vma->vm_end <= USER_VM_START)
		{
			continue;
		}
		if (vma->vm_start >= USER_VM_END)
		{
			break;
		}
		if (vma->vm_start >= addr + len)
		{
			return addr;
		}
		if (vma->vm_end > addr)
		{
			addr = vma->vm_end;
		}
	}

	if (addr + len <= USER_VM_END)
	{
		return addr;
	}

	printk(KERN_WARNING "get_unmapped_area_user: no space for %lu bytes in user range\n", (unsigned long)len);
	return 0;
}

/** VMAリストを別mm_structへ複製する
 * @param dst 複製先のメモリディスクリプタ
 * @param src 複製元のメモリディスクリプタ
 * @return 成功時0，失敗時-1
 */
int clone_vm_areas(struct mm_struct *dst, const struct mm_struct *src)
{
	struct vm_area_struct *vma;
	struct vm_area_struct *copy;

	if (dst == NULL || src == NULL)
	{
		return -1;
	}

	for (vma = src->mmap; vma != NULL; vma = vma->vm_next)
	{
		copy = (struct vm_area_struct *)kmalloc(sizeof(*copy));
		if (copy == NULL)
		{
			free_vm_areas(dst);
			return -1;
		}
		memcpy(copy, vma, sizeof(*copy));
		copy->vm_next = NULL;
		if (insert_vm_area(dst, copy) != 0)
		{
			kfree(copy);
			free_vm_areas(dst);
			return -1;
		}
	}
	return 0;
}

/** mm_structに紐づくVMAメタデータを解放する
 * @param mm 解放対象のメモリディスクリプタ
 */
void free_vm_areas(struct mm_struct *mm)
{
	struct vm_area_struct *vma;
	struct vm_area_struct *next;

	if (mm == NULL)
	{
		return;
	}

	for (vma = mm->mmap; vma != NULL; vma = next)
	{
		next = vma->vm_next;
		kfree(vma);
	}
	mm->mmap = NULL;
}

/** mm_structを初期化する
 * @param mm 初期化対象のメモリディスクリプタ
 * @param pgd 対応するページディレクトリ
 */
void mm_init(struct mm_struct *mm, pgd_t *pgd)
{
	if (mm == NULL)
	{
		return;
	}
	memset(mm, 0, sizeof(*mm));
	mm->pgd = pgd;
	mm->mm_count.counter = 1;
}

/** プロセス用mm_structとPGDを割り当てる
 * @param source_pgd コピー元PGD，NULLならkernel_pgd()を使う
 * @return 割り当てたmm_struct，失敗時NULL
 */
struct mm_struct *mm_alloc(pgd_t *source_pgd)
{
	struct mm_struct *mm;
	struct page *pgd_page;
	pgd_t *pgd;

	mm = (struct mm_struct *)kmalloc(sizeof(*mm));
	if (mm == NULL)
	{
		return NULL;
	}

	pgd_page = alloc_pages(GFP_KERNEL | GFP_ZERO, 0);
	if (pgd_page == NULL)
	{
		kfree(mm);
		return NULL;
	}

	pgd = (pgd_t *)pgd_page;
	if (copy_page_tables(pgd, source_pgd ? source_pgd : kernel_pgd()) != 0)
	{
		free_pages(pgd_page, 0);
		kfree(mm);
		return NULL;
	}

	mm_init(mm, pgd);
	return mm;
}

/** mm_structと対応するページテーブルを破棄する
 * @param mm 破棄対象のメモリディスクリプタ
 */
void mm_destroy(struct mm_struct *mm)
{
	if (mm == NULL)
	{
		return;
	}

	/* ユーザ物理ページは所有VMAをmunmapする経路で明示的に解放する */
	free_vm_areas(mm);
	if (mm->pgd != NULL && mm->pgd != kernel_pgd())
	{
		free_page_tables(mm->pgd);
	}
	kfree(mm);
}

/** テスト用にVMA管理状態を初期状態へ戻す */
void vm_reset_for_test(void)
{
	kernel_vm_area_list = NULL;
	next_vm_addr = KERNEL_VM_START;
}
