#include <stdint.h>
/* Shared I/O override so multiple test translation units can link */
struct io_log_entry
{
	uint16_t port;
	uint8_t val;
	int is_out;
};
struct io_log_entry kfs_io_log_entries[1024];
int kfs_io_log_count;

void kfs_io_outb(uint16_t port, uint8_t val)
{
	if (kfs_io_log_count < 1024)
		kfs_io_log_entries[kfs_io_log_count++] = (struct io_log_entry){port, val, 1};
}
uint8_t kfs_io_inb(uint16_t port)
{
	(void)port;
	return 0x20;
}
