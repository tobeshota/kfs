#ifndef KFS_TERMINAL_TEST_SUPPORT_H
#define KFS_TERMINAL_TEST_SUPPORT_H
#include <stddef.h>
#include <stdint.h>

/* Externs exposing internal terminal state for tests (not for production use). */
extern size_t kfs_terminal_row;
extern size_t kfs_terminal_column;
extern uint8_t kfs_terminal_color;
extern uint16_t *kfs_terminal_buffer;

/* Test helper APIs (mirror old getters) */
static inline void kfs_terminal_set_buffer(uint16_t *buf)
{
	extern uint16_t *kfs_terminal_buffer; /* redundancy safe */
	if (buf)
		kfs_terminal_buffer = buf;
}
static inline uint16_t *kfs_terminal_get_buffer(void)
{
	return kfs_terminal_buffer;
}
static inline size_t kfs_terminal_get_row(void)
{
	return kfs_terminal_row;
}
static inline size_t kfs_terminal_get_col(void)
{
	return kfs_terminal_column;
}
static inline uint8_t kfs_terminal_get_color(void)
{
	return kfs_terminal_color;
}

#endif /* KFS_TERMINAL_TEST_SUPPORT_H */
