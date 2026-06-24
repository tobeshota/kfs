#ifndef _KFS_MMAN_H
#define _KFS_MMAN_H

#include <kfs/stddef.h> /* size_t */

struct mm_struct;

/* mmap prot フラグ */
#define PROT_NONE 0x0  /* アクセス不可 */
#define PROT_READ 0x1  /* 読み取り可能 */
#define PROT_WRITE 0x2 /* 書き込み可能 */
#define PROT_EXEC 0x4  /* 実行可能 */

/* mmap flags */
#define MAP_SHARED 0x01	   /* 複数プロセスで共有（現実装では未サポート） */
#define MAP_PRIVATE 0x02   /* プロセス固有のプライベートマッピング */
#define MAP_FIXED 0x10	   /* 指定アドレスに強制マッピング（現実装では未サポート） */
#define MAP_ANONYMOUS 0x20 /* ファイルに関連しない匿名マッピング */
#define MAP_ANON MAP_ANONYMOUS

/* do_mmap / munmap の失敗戻り値 */
#define MAP_FAILED ((void *)-1)

void *do_mmap_mm(struct mm_struct *mm, void *addr, unsigned long len, int prot, int flags);
int do_munmap_mm(struct mm_struct *mm, unsigned long addr, unsigned long len);
void *do_mmap(void *addr, unsigned long len, int prot, int flags);
int do_munmap(unsigned long addr, unsigned long len);
void *sys_mmap2(unsigned long addr, unsigned long len, int prot, int flags, int fd, unsigned long pgoff);
int sys_munmap(unsigned long addr, unsigned long len);

#endif /* _KFS_MMAN_H */
