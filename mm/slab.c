#include <asm-i386/page.h>
#include <kfs/gfp.h>
#include <kfs/mm.h>
#include <kfs/panic.h>
#include <kfs/printk.h>
#include <kfs/slab.h>
#include <kfs/stdint.h>

/* オブジェクトメタデータ（各オブジェクトの先頭に配置） */
struct obj_meta
{
	/** このメモリがSlabアロケータによって管理されているかを識別するためのもの
	 * @details
	 * 以下の用途を持つ:
	 * 1. このメモリがSlabアロケータで割り当てられたものかを識別する
	 * 2. このメモリがSlabアロケータ管理のどのキャッシュ（32B/64B/128B...）に属するかを特定する
	 *
	 * @example
	 * meta->magic = SLAB_MAGIC | cache_idx; のように設定し，
	 * int check = ((meta->magic & 0xFFFF0000) == SLAB_MAGIC) のようにチェックする．
	 * ここで cache_idx は識別対象のキャッシュインデックスである．
	 * - 上位16ビット (0x5AB0): Slab識別用マジック番号
	 * - 下位16ビット: キャッシュインデックス (0-7)
	 *
	 * kfree()時にマジック番号をチェックして不正なポインタを検出し，
	 * ksize()でキャッシュインデックスからサイズを逆引きできる．
	 *
	 * @note obj_metaはkmem_cache_nodeと同じ領域を共用する設計になっている
	 * @see include/kfs/slab.h の kmem_cache_node コメント
	 */
	uint32_t magic;

	uint32_t reserved; /* 予約（将来の拡張用） */
};

#define OBJ_META_SIZE sizeof(struct obj_meta)
#define SLAB_MAGIC 0x5AB00000 /* "SLAB" のような値 */

/* キャッシュサイズ定義 (Linux 2.6.11のkmalloc_sizesに相当) */
#define KMALLOC_MIN_SIZE 32
#define KMALLOC_MAX_SIZE 4096
#define NR_CACHES 8

/** 固定サイズキャッシュ
 * @note Linux 2.6.11のcache_sizes配列に相当する
 */
static struct kmem_cache kmalloc_caches[NR_CACHES] = {
	{"kmalloc-32", 32, NULL, 0, 0},		{"kmalloc-64", 64, NULL, 0, 0},		{"kmalloc-128", 128, NULL, 0, 0},
	{"kmalloc-256", 256, NULL, 0, 0},	{"kmalloc-512", 512, NULL, 0, 0},	{"kmalloc-1024", 1024, NULL, 0, 0},
	{"kmalloc-2048", 2048, NULL, 0, 0}, {"kmalloc-4096", 4096, NULL, 0, 0},
};

/* 各キャッシュ kmem_cache[cache_idx] が管理するページ数 */
static unsigned long cache_pages[NR_CACHES] = {0};

/* カーネルヒープの境界管理 */
static unsigned long kernel_heap_start = 0; /* ヒープ開始アドレス */
static unsigned long kernel_heap_brk = 0;	/* 現在のブレイクポイント */
static unsigned long kernel_heap_limit = 0; /* ヒープ上限 */
static int slab_initialized = 0;			/* 初期化済みフラグ */

#define KERNEL_HEAP_SIZE (1 * 1024 * 1024) /* 初期ヒープサイズ: 1MB */

/** size以上のキャッシュkmalloc_cachesのインデックスを返す
 * @param size 要求サイズ
 * @return 見つかったキャッシュのインデックス、見つからなければ-1
 * @details 要求サイズを満たす最小のキャッシュを選ぶ (Best Fit)
 * @example size=50 ならば 64Bのキャッシュを選択する
 */
static int find_cache_index(size_t size)
{
	int i;

	for (i = 0; i < NR_CACHES; i++)
	{
		/* 要求サイズ以上の最初のキャッシュを返す */
		if (kmalloc_caches[i].size >= size)
		{
			return i;
		}
	}

	/* サイズが大きすぎる */
	return -1;
}

/** キャッシュに新しいページを追加する
 * @param cache 対象キャッシュ
 * @param cache_idx 対象キャッシュのインデックス
 * @details 物理ページを割り当て，オブジェクト単位に分割して未割当リストに追加する
 */
static int kmem_cache_grow(struct kmem_cache *cache, int cache_idx)
{
	struct page *page;
	unsigned long addr; /* 物理ページのアドレス */
	size_t nr_objs;		/* number of objects */
	size_t i;
	struct kmem_cache_node *node;

	/* 1つの物理ページ(PAGE_SIZEバイト)を割り当てる */
	page = alloc_pages(GFP_KERNEL, 0);
	if (!page)
	{
		printk("kmem_cache_grow: failed to allocate page for %s\n", cache->name);
		return -1;
	}

	/* 物理ページのアドレスを取得 */
	addr = (unsigned long)page;

	/* キャッシュのアドレス範囲を更新（最初の物理ページなら初期化） */
	if (cache->start_addr == 0)
	{
		cache->start_addr = addr;
		cache->end_addr = addr + PAGE_SIZE;
	}
	else
	{
		/** 新しい物理ページがキャッシュの既存範囲より前/後にある場合、範囲を広げる
		 * @example
		 * 既存範囲が0x3000-0x4000で，新しい物理ページが0x2000ならば，0x2000-0x4000に拡張する
		 * 既存範囲が0x2000-0x3000で，新しい物理ページが0x4000ならば，0x2000-0x5000に拡張する
		 */
		if (addr < cache->start_addr)
		{
			cache->start_addr = addr;
		}
		if (addr + PAGE_SIZE > cache->end_addr)
		{
			cache->end_addr = addr + PAGE_SIZE;
		}
	}

	/** 物理ページをキャッシュサイズcache->sizeで分割
	 * @example 32バイトキャッシュの場合:
	 * PAGE_SIZE(4096バイト) / 32バイト = 128個のオブジェクト
	 */
	nr_objs = PAGE_SIZE / cache->size;

	/* 物理ページをキャッシュサイズcache->sizeで分割した各nodeを未割当リストfreelistに追加 */
	for (i = 0; i < nr_objs; i++)
	{
		node = (struct kmem_cache_node *)((i * cache->size) + addr);

		/** node->addrはメタデータの後(ユーザ領域)を指すようにする
		 *
		 * @details
		 * - nodeはメタデータを指す
		 *   (ユーザに割り当てたkmem_cache_node* nodeがkmalloc()内でobj_meta型として用いられるため)
		 * - そこで，node->addrはメタデータの後(ユーザ領域)を指すようにする
		 *   こうすれば，kmalloc()が返すポインタはユーザ領域を指し，ユーザはメタデータを意識する必要がなくなる
		 *
		 * アドレス 0x100000:
		 * ┌──────────────┬─────────────────────────┐
		 * │ メタデータ     │  ユーザー領域             │
		 * │   8バイト     │    24バイト              │
		 * └──────────────┴─────────────────────────┘
		 * 	↑(0x100000)    ↑(0x100008)
		 *  node           node->addr
		 */
		node->addr = (void *)((char *)node + OBJ_META_SIZE);

		/* 未割当リストfreelistの先頭にnodeを挿入する */
		node->next = cache->freelist; /* 現在の未割当リストの先頭を新しいノードの次にする */
		cache->freelist = node;		  /* 未割当リストの先頭を新しいノードにする */
	}

	/* cache_idx番目のキャッシュkmalloc_caches[cache_idx]が管理する物理ページ数を増やす */
	cache_pages[cache_idx]++;

	return 0;
}

/** slabアロケータを初期化する
 * @details 各固定サイズキャッシュkmalloc_cachesに初期ページを割り当てる
 */
void kmem_cache_init(void)
{
	int i;
	struct page *heap_pages;

	/* 既に初期化済みなら何もしない */
	if (slab_initialized)
	{
		return;
	}

	printk("Initializing slab allocator...\n");

	/* カーネルヒープ領域を初期化 */
	heap_pages = alloc_pages(GFP_KERNEL, 0);
	if (!heap_pages)
	{
		panic("kmem_cache_init: failed to allocate heap");
	}

	/* ヒープ境界を設定 */
	kernel_heap_start = (unsigned long)heap_pages;
	kernel_heap_brk = kernel_heap_start;
	kernel_heap_limit = kernel_heap_start + KERNEL_HEAP_SIZE;

	printk("  Kernel heap: 0x%08lx - 0x%08lx (%lu KB)\n", kernel_heap_start, kernel_heap_limit,
		   KERNEL_HEAP_SIZE / 1024);

	/* 各固定サイズキャッシュkmalloc_cachesに初期ページを割り当てる */
	for (i = 0; i < NR_CACHES; i++)
	{
		if (kmem_cache_grow(&kmalloc_caches[i], i) < 0)
		{
			panic("kmem_cache_init: failed to grow cache %s", kmalloc_caches[i].name);
		}
		printk("  %s: initialized with %lu pages\n", kmalloc_caches[i].name, cache_pages[i]);
	}

	printk("Slab allocator initialized\n");
	slab_initialized = 1;
}

/** カーネルメモリを割り当てる
 * @param size 割り当てるバイト数
 * @return 割り当てられたメモリへのポインタ、失敗時NULL
 * @details
 * 適切なサイズのキャッシュkmalloc_cachesを選び，その未割当リストから1つ取り出す．
 * リストが空なら新しいページを追加してから取り出す
 */

void *kmalloc(size_t size)
{
	int idx;
	struct kmem_cache *cache;
	struct kmem_cache_node *node;
	void *ptr;

	/* サイズチェック */
	if (size == 0)
	{
		/* サイズ0は割り当てない */
		return NULL;
	}
	if (size > KMALLOC_MAX_SIZE) // サイズが大きすぎる
	{
		printk("kmalloc: size %lu too large (max %d)\n", (unsigned long)size, KMALLOC_MAX_SIZE);
		return NULL;
	}

	/* size以上のキャッシュkmalloc_cachesのインデックスを返す */
	idx = find_cache_index(size);
	if (idx < 0)
	{
		printk("kmalloc: no suitable cache for size %lu\n", (unsigned long)size);
		return NULL;
	}
	cache = &kmalloc_caches[idx];

	/* 未割当リストが空なら新しいページを追加する */
	if (!cache->freelist)
	{
		if (kmem_cache_grow(cache, idx) < 0)
		{
			printk("kmalloc: failed to grow cache %s\n", cache->name);
			return NULL;
		}
	}

	/* 未割当リストから先頭を取り出す */
	node = cache->freelist; // 先頭の未割当リストを取得する
	ptr = node->addr;
	// 未割当リストの先頭を次のノードにし，nodeをkmalloc_caches[cache_idx].freelistから外す
	// (nodeはこれから使用済みになるため)
	cache->freelist = node->next;

	/** メタデータを設定する
	 * ここで設定した obj_meta *meta はkfree()やksize()等で使用される
	 */
	// kmem_cache_node *型の構造体 node を obj_meta *型の構造体 として用いる
	struct obj_meta *meta = (struct obj_meta *)node;
	meta->magic = SLAB_MAGIC | idx;
	meta->reserved = 0;

	return ptr;
}

/** カーネルメモリを解放する
 * @param ptr 解放するメモリへのポインタ
 * @details オブジェクトを未割当リストに戻す
 * kmalloc()にて使用済みとしてkmalloc_caches[cache_idx].freelistから外していた
 * ptr指定のnodeをkmalloc_caches[cache_idx].freelistに戻す
 */
void kfree(void *ptr)
{
	struct obj_meta *meta;
	struct kmem_cache_node *node;
	int cache_idx;

	/* NULLポインタは何もしない */
	if (!ptr)
	{
		return;
	}

	/* メタデータを取得 */
	meta = (struct obj_meta *)((char *)ptr - OBJ_META_SIZE);

	/* マジック番号を確認 */
	if ((meta->magic & 0xFFFF0000) != SLAB_MAGIC)
	{
		printk("kfree: invalid pointer %p (bad magic 0x%lx)\n", ptr, (unsigned long)meta->magic);
		return;
	}

	/* キャッシュインデックスを取得 */
	cache_idx = meta->magic & 0xFFFF;
	if (cache_idx >= NR_CACHES)
	{
		printk("kfree: invalid cache index %d\n", cache_idx);
		return;
	}

	/* kmalloc()にて使用済みとしてkmalloc_caches[cache_idx].freelistから外していたptr指定のnodeを
	 * kmalloc_caches[cache_idx].freelistの先頭に挿入し，
	 * 次回kmalloc()呼び出したとき，ptr指定の未割当リストが
	 * 1番目に割り当てられるようにする．
	 */
	// obj_meta *型の構造体 meta を kmem_cache_node *型の構造体 として用いる
	node = (struct kmem_cache_node *)meta;
	node->addr = ptr; /* ユーザーポインタを保存 */
	node->next = kmalloc_caches[cache_idx].freelist;
	kmalloc_caches[cache_idx].freelist = node;
}

/** 割り当てられたメモリのサイズを取得する
 * @param ptr メモリへのポインタ
 * @return 割り当てられたバイト数
 * @details オブジェクトが属するキャッシュのサイズを返す
 */
size_t ksize(void *ptr)
{
	struct obj_meta *meta;
	int cache_idx;

	/* NULLポインタは0 */
	if (!ptr)
	{
		return 0;
	}

	/* メタデータを取得（ユーザーポインタの直前） */
	meta = (struct obj_meta *)((char *)ptr - OBJ_META_SIZE);

	/* マジック番号を確認 */
	if ((meta->magic & 0xFFFF0000) != SLAB_MAGIC)
	{
		return 0;
	}

	/* キャッシュインデックスを取得 */
	cache_idx = meta->magic & 0xFFFF;
	if (cache_idx >= NR_CACHES)
	{
		return 0;
	}

	/* キャッシュサイズを返す */
	return kmalloc_caches[cache_idx].size;
}

/** カーネルヒープのブレイクポイントを変更する
 * @param increment 増減するバイト数（正で増加、負で減少）
 * @return 新しいブレイクポイント、失敗時NULL
 * @details ヒープ領域の境界を変更し動的にメモリ領域を拡張・縮小する
 * @note Linux 2.6.11のbrkに相当する
 */
void *kbrk(intptr_t increment)
{
	unsigned long new_brk;
	unsigned long old_brk;

	/* ヒープが初期化されていない */
	if (kernel_heap_start == 0)
	{
		printk("kbrk: heap not initialized\n");
		return NULL;
	}

	old_brk = kernel_heap_brk;
	new_brk = old_brk + increment;

	/* 境界チェック: ヒープ開始位置より前には戻せない */
	if (new_brk < kernel_heap_start)
	{
		printk("kbrk: new_brk (0x%08lx) < heap_start (0x%08lx)\n", new_brk, kernel_heap_start);
		return NULL;
	}

	/* 境界チェック: ヒープ上限を超えられない */
	if (new_brk > kernel_heap_limit)
	{
		printk("kbrk: new_brk (0x%08lx) > heap_limit (0x%08lx)\n", new_brk, kernel_heap_limit);
		return NULL;
	}

	/* ブレイクポイントを更新 */
	kernel_heap_brk = new_brk;

	/* 新しいブレイクポイントを返す */
	return (void *)new_brk;
}

/** テスト用: Slabアロケータを初期状態にリセット
 * @details
 * 各キャッシュを初期化直後の状態に完全リセットする。
 * テストの独立性を保証するために、各テスト前に呼び出す。
 *
 * リセット内容:
 * - 各キャッシュのフリーリストを初期状態に再構築
 * - 割り当てカウンタをリセット
 * - 追加割り当てされたページは保持（シンプルさ優先）
 */
void kmem_cache_reset_for_test(void)
{
	int i;
	struct kmem_cache *cache;

	/* 初期化されていなければ何もしない */
	if (!slab_initialized)
	{
		return;
	}

	/* 各キャッシュを初期状態にリセット */
	for (i = 0; i < NR_CACHES; i++)
	{
		cache = &kmalloc_caches[i];

		/* フリーリストをクリア */
		cache->freelist = NULL;

		/* キャッシュを再構築（最初の1ページで初期化） */
		if (kmem_cache_grow(cache, i) < 0)
		{
			panic("kmem_cache_reset_for_test: failed to regrow cache %s", cache->name);
		}
	}

	/* ヒープのブレイクポイントを初期位置にリセット */
	kernel_heap_brk = kernel_heap_start;
}
