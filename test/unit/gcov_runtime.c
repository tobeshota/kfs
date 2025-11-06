/* GCOV runtime for freestanding environment (unit test only)
 *
 * In a freestanding environment, we only need to provide __gcov_exit()
 * because it's normally registered with atexit() which doesn't exist here.
 * All other gcov functions (__gcov_init, __gcov_merge_add, __gcov_dump, etc.)
 * are already provided by GCC's libgcov.a which we link with -lgcov.
 */

#include <kfs/printk.h>

/* External functions provided by libgcov */
extern void __gcov_dump(void);

/**
 * __gcov_exit - Called at program exit to dump coverage data
 *
 * In a normal hosted environment, this is registered with atexit().
 * In our freestanding environment, we need to provide this function
 * because GCC-generated code calls it, but we don't have atexit().
 *
 * We simply delegate to __gcov_dump() which is provided by libgcov.
 */
void __gcov_exit(void)
{
	__gcov_dump();
}

/**
 * gcov_kernel_dump - Manual coverage dump function
 *
 * Call this function before kernel exit to ensure coverage data is saved.
 * This is a convenience wrapper for test code.
 */
void gcov_kernel_dump(void)
{
	printk("GCOV: Dumping coverage data...\n");
	__gcov_exit();
	printk("GCOV: Coverage dump complete\n");
}
