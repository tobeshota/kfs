#ifndef _KFS_NEOFETCH_H
#define _KFS_NEOFETCH_H

struct kfs_neofetch_info
{
	unsigned long total_mem_mib;
	unsigned long used_mem_mib;
	unsigned long free_mem_mib;
	unsigned long kernel_mem_mib;
};

void print_neofetch(void);

#endif /* _KFS_NEOFETCH_H */
