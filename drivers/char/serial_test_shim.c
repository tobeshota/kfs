#include <stdint.h>
#include <stddef.h>

/* Test shim for serial I/O: logs outb/inb operations instead of executing
 * port I/O. Only compiled for unit tests, never for production build. */

#define COM1_PORT 0x3F8

struct kfs_test_io_log_entry { uint16_t port; uint8_t value; int is_out; };
static struct kfs_test_io_log_entry kfs_test_io_log[256];
static int kfs_test_io_log_count;

static inline void outb(uint16_t port, uint8_t val) {
    if (kfs_test_io_log_count < (int)(sizeof(kfs_test_io_log)/sizeof(kfs_test_io_log[0])))
        kfs_test_io_log[kfs_test_io_log_count++] = (struct kfs_test_io_log_entry){port,val,1};
}
static inline uint8_t inb(uint16_t port) { (void)port; return 0x20; }

/* Re-implement subset of production API for tests */
static int serial_transmit_empty(void) { return inb(COM1_PORT + 5) & 0x20; }
void serial_init(void) {
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x80);
    outb(COM1_PORT + 0, 0x03);
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x03);
    outb(COM1_PORT + 2, 0xC7);
    outb(COM1_PORT + 4, 0x0B);
}
void serial_write(const char *data, size_t size) {
    for (size_t i=0;i<size;i++) {
        char c = data[i];
        if (c == '\n') { while(!serial_transmit_empty()){} outb(COM1_PORT + 0, (uint8_t)'\r'); }
        while(!serial_transmit_empty()){}
        outb(COM1_PORT + 0, (uint8_t)c);
    }
}

int kfs_test_get_io_log_count(void) { return kfs_test_io_log_count; }
const struct kfs_test_io_log_entry* kfs_test_get_io_log(void) { return kfs_test_io_log; }
void kfs_test_clear_io_log(void) { kfs_test_io_log_count = 0; }
