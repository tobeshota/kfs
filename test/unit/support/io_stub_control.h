#ifndef KFS_IO_STUB_CONTROL_H
#define KFS_IO_STUB_CONTROL_H

#include <stddef.h>
#include <stdint.h>

void kfs_stub_reset_io(void);
void kfs_stub_set_serial_status_sequence(const uint8_t *seq, size_t len, uint8_t default_status);
size_t kfs_stub_serial_status_remaining(void);
void kfs_stub_push_keyboard_scancode(uint8_t scancode);
size_t kfs_stub_keyboard_queue_size(void);

#endif /* KFS_IO_STUB_CONTROL_H */
