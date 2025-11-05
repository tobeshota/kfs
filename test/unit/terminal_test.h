#ifndef KFS_TEST_TERMINAL_TEST_H
#define KFS_TEST_TERMINAL_TEST_H

#include <kfs/stddef.h>
#include <kfs/stdint.h>

/* Test-only accessors & helpers for terminal / VGA layer.
 * These are intentionally separated from production headers so that
 * production builds are not influenced by test instrumentation.
 */
uint16_t kfs_test_vga_entry(unsigned char uc, uint8_t color);
uint8_t kfs_test_vga_entry_color(uint8_t fg, uint8_t bg);
size_t kfs_test_strlen(const char *s);
void kfs_test_term_init(void);
void kfs_test_term_putc(char c);
void kfs_test_term_write(const char *s);
size_t kfs_term_get_row(void);
size_t kfs_term_get_col(void);
uint8_t kfs_term_get_color(void);
uint16_t kfs_term_peek(size_t x, size_t y);

#endif /* KFS_TEST_TERMINAL_TEST_H */
