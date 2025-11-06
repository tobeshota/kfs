/* GCOV runtime interface for freestanding environment (unit test only) */

#ifndef _GCOV_RUNTIME_H
#define _GCOV_RUNTIME_H

/**
 * gcov_kernel_dump - Dump all coverage data
 *
 * Call this function before kernel exit to ensure coverage data is saved.
 * This should be called from the test framework's cleanup code.
 */
void gcov_kernel_dump(void);

#endif /* _GCOV_RUNTIME_H */
