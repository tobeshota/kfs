/** 仮想アドレス空間の管理を行う
 * - 仮想メモリ領域（VMA）の作成・削除・検索
 * - カーネル空間とユーザー空間の管理
 */

#include <asm-i386/page.h>
#include <kfs/mm.h>
#include <kfs/printk.h>
#include <kfs/stddef.h>

/* 仮想メモリ領域のリスト（カーネル用） */
/* テスト用にstaticを外してエクスポート */
struct vm_area_struct *vm_area_list = NULL;
#define vma_list vm_area_list /* 内部コードとの互換性のためエイリアス */

/* カーネル仮想メモリの開始位置（ページング後の高位メモリ） */
/* Linux 2.6.11では VMALLOC_START に相当 */
#define KERNEL_VM_START 0xD0000000 /* 3.25GB */
#define KERNEL_VM_END 0xFFFFFFFF   /* 4GB */

/* 次に割り当て可能な仮想アドレス */
static unsigned long next_vm_addr = KERNEL_VM_START;

/**
 * 指定したアドレスを含む仮想メモリ領域を検索
 * Linux 2.6.11の find_vma() に相当
 *
 * @param addr 検索する仮想アドレス
 * @return 見つかったVMA、見つからない場合はNULL
 */
struct vm_area_struct *find_vma(unsigned long addr)
{
	struct vm_area_struct *vma;

	for (vma = vma_list; vma != NULL; vma = vma->vm_next)
	{
		if (addr >= vma->vm_start && addr < vma->vm_end)
		{
			return vma;
		}
	}

	return NULL;
}

/**
 * 仮想メモリ領域をリストに挿入
 * アドレス順にソートして挿入
 *
 * @param new_vma 挿入するVMA
 * @return 成功時0、失敗時-1
 */
int insert_vm_area(struct vm_area_struct *new_vma)
{
	struct vm_area_struct *vma, *prev;

	if (new_vma == NULL)
	{
		return -1;
	}

	/* 重複チェック */
	for (vma = vma_list; vma != NULL; vma = vma->vm_next)
	{
		/* 新しいVMAが既存のVMAと重複していないかチェック */
		if (new_vma->vm_start < vma->vm_end && new_vma->vm_end > vma->vm_start)
		{
			printk(KERN_WARNING "insert_vm_area: overlap detected\n");
			return -1;
		}
	}

	/* リストが空の場合 */
	if (vma_list == NULL)
	{
		vma_list = new_vma;
		new_vma->vm_next = NULL;
		return 0;
	}

	/* 先頭に挿入する場合 */
	if (new_vma->vm_start < vma_list->vm_start)
	{
		new_vma->vm_next = vma_list;
		vma_list = new_vma;
		return 0;
	}

	/* アドレス順に挿入位置を探す */
	prev = vma_list;
	for (vma = vma_list->vm_next; vma != NULL; vma = vma->vm_next)
	{
		if (new_vma->vm_start < vma->vm_start)
		{
			/* prevとvmaの間に挿入 */
			prev->vm_next = new_vma;
			new_vma->vm_next = vma;
			return 0;
		}
		prev = vma;
	}

	/* リストの最後に挿入 */
	prev->vm_next = new_vma;
	new_vma->vm_next = NULL;
	return 0;
}

/**
 * 指定したアドレスの仮想メモリ領域をリストから削除
 *
 * @param addr 削除するVMAの開始アドレス
 */
void remove_vm_area(unsigned long addr)
{
	struct vm_area_struct *vma, *prev;

	if (vma_list == NULL)
	{
		return;
	}

	/* 先頭の要素を削除する場合 */
	if (vma_list->vm_start == addr)
	{
		vma_list = vma_list->vm_next;
		return;
	}

	/* リストを走査して削除 */
	prev = vma_list;
	for (vma = vma_list->vm_next; vma != NULL; vma = vma->vm_next)
	{
		if (vma->vm_start == addr)
		{
			prev->vm_next = vma->vm_next;
			return;
		}
		prev = vma;
	}
}

/**
 * 指定サイズの未使用仮想アドレス領域を見つける
 * First Fit方式で検索
 *
 * @param len 必要なサイズ（バイト単位）
 * @return 使用可能な仮想アドレス、見つからない場合は0
 */
unsigned long get_unmapped_area(size_t len)
{
	struct vm_area_struct *vma;
	unsigned long addr;

	/* サイズをページ境界に切り上げ */
	len = (len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	/* リストが空の場合は開始位置を返す */
	if (vma_list == NULL)
	{
		if (next_vm_addr + len <= KERNEL_VM_END)
		{
			addr = next_vm_addr;
			next_vm_addr += len;
			return addr;
		}
		return 0;
	}

	/* 既存のVMA間の隙間を探す */
	addr = KERNEL_VM_START;
	for (vma = vma_list; vma != NULL; vma = vma->vm_next)
	{
		/* 現在位置とVMAの間に十分なスペースがあるか */
		if (vma->vm_start >= addr + len)
		{
			return addr;
		}
		/* 次の候補位置は現在のVMAの終端 */
		addr = vma->vm_end;
	}

	/* リストの最後にスペースがあるか */
	if (addr + len <= KERNEL_VM_END)
	{
		return addr;
	}

	/* 見つからない */
	return 0;
}
