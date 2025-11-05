/**
 * Paging subsystem stubs for unit testing
 *
 * arch/i386/mm/init.c functions are not included in unit tests,
 * so we provide stub implementations here.
 */

#include <asm-i386/pgtable.h>
#include <kfs/stddef.h>

/**
 * paging_init stub - no-op for unit tests
 */
void paging_init(void)
{
	/* No-op: paging is not enabled in unit test environment */
}

/**
 * get_pte stub - returns NULL for unit tests
 */
pte_t *get_pte(unsigned long vaddr)
{
	(void)vaddr;
	return NULL;
}

/**
 * map_page stub - always returns success for unit tests
 */
int map_page(unsigned long vaddr, unsigned long paddr, unsigned long flags)
{
	(void)vaddr;
	(void)paddr;
	(void)flags;
	return 0;
}