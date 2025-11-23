#ifndef _KFS_SLAB_H
#define _KFS_SLAB_H

#include <kfs/stddef.h>
#include <kfs/stdint.h>

/** 物理ページをキャッシュサイズ(cache->size)で分割した各ノード
 * @details
 * kmem_cache_nodeは
 * - 割当前: kmem_cache_node構造体として，未割当リスト(freelist)を形成する
 * - 割当後: obj_meta構造体として，ユーザに割り当てたノードのメタデータを格納する
 *
 * 【割当前】アドレス 0x100000:
 * ┌────────────┬────────────┐
 * │ addr       │ next       │ kmem_cache_nodeとして使用
 * │ 0x100008   │ 0x100020   │
 * └────────────┴────────────┘
 *    ↓ kmalloc()　↑kfree()
 * 【割当後】アドレス 0x100000:
 * ┌────────────┬────────────┐
 * │ magic      │ reserved   │ obj_metaとして使用
 * │ 0x5AB00001 │ 0x00000000 │ (kmem_cache_nodeとして使用した領域は上書きされる)
 * └────────────┴────────────┘
 *
 * @see mm/slab.c
 * - kmalloc() の struct obj_meta *meta = (struct obj_meta *)node; より
 *   kmem_cache_node *型の構造体 node を obj_meta *型の構造体 として用いている
 * - kfree() の node = (struct kmem_cache_node *)meta; より
 *   obj_meta *型の構造体 meta を kmem_cache_node *型の構造体 として用いている
 *
 * @note
 * - kmem_cache_nodeをobj_metaと共用する設計にした理由は
 *   メモリ効率化のため(未割当ノードのメタデータ領域を確保しないようにするため)である．
 * - kmem_cache_nodeは，Linux 2.6.11のkmem_bufctlに相当する
 */
struct kmem_cache_node
{
	void *addr;					  /* ユーザ領域アドレス */
	struct kmem_cache_node *next; /* 次のノードへのポインタ */
};

/* キャッシュ記述子 (Linux 2.6.11のkmem_cacheに相当) */
struct kmem_cache
{
	const char *name;				  /* キャッシュ名 */
	size_t size;					  /* オブジェクトサイズ */
	struct kmem_cache_node *freelist; /* 未割当リストの先頭 */
	unsigned long start_addr;		  /* このキャッシュの開始アドレス */
	unsigned long end_addr;			  /* このキャッシュの終了アドレス */
};

void kmem_cache_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);
size_t ksize(void *ptr);
void *kbrk(intptr_t increment);

/* テスト用リセット関数 */
void kmem_cache_reset_for_test(void);

#endif /* _KFS_SLAB_H */
