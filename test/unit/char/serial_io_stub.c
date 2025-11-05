#include <kfs/stddef.h>
#include <kfs/stdint.h>

#define STUB_COM1_PORT 0x3F8
#define STUB_COM1_STATUS (STUB_COM1_PORT + 5)
#define STUB_PS2_STATUS 0x64
#define STUB_PS2_DATA 0x60

/* Shared I/O override so multiple test translation units can link */
struct io_log_entry
{
	uint16_t port;
	uint8_t val;
	int is_out;
};
static const size_t KFS_IO_LOG_CAPACITY = 8192;
struct io_log_entry kfs_io_log_entries[8192];
int kfs_io_log_count;

static uint8_t serial_status_seq[64];
static size_t serial_status_len;
static size_t serial_status_pos;
static uint8_t serial_status_default = 0x20;

static uint8_t keyboard_queue[256];
static size_t keyboard_head;
static size_t keyboard_tail;

void kfs_stub_reset_io(void)
{
	kfs_io_log_count = 0;
	serial_status_len = 0;
	serial_status_pos = 0;
	serial_status_default = 0x20;
	keyboard_head = 0;
	keyboard_tail = 0;
}

void kfs_stub_set_serial_status_sequence(const uint8_t *seq, size_t len, uint8_t default_status)
{
	if (!seq)
	{
		serial_status_len = 0;
		serial_status_pos = 0;
		serial_status_default = default_status;
		return;
	}
	if (len > sizeof(serial_status_seq))
		len = sizeof(serial_status_seq);
	for (size_t i = 0; i < len; i++)
		serial_status_seq[i] = seq[i];
	serial_status_len = len;
	serial_status_pos = 0;
	serial_status_default = default_status;
}

size_t kfs_stub_serial_status_remaining(void)
{
	if (serial_status_pos >= serial_status_len)
		return 0;
	return serial_status_len - serial_status_pos;
}

void kfs_stub_push_keyboard_scancode(uint8_t scancode)
{
	if (keyboard_tail < sizeof(keyboard_queue))
		keyboard_queue[keyboard_tail++] = scancode;
}

size_t kfs_stub_keyboard_queue_size(void)
{
	return keyboard_tail >= keyboard_head ? (keyboard_tail - keyboard_head) : 0;
}

void kfs_io_outb(uint16_t port, uint8_t val)
{
	if (kfs_io_log_count < (int)KFS_IO_LOG_CAPACITY)
		kfs_io_log_entries[kfs_io_log_count++] = (struct io_log_entry){port, val, 1};
}

uint8_t kfs_io_inb(uint16_t port)
{
	if (port == STUB_COM1_STATUS)
	{
		if (serial_status_pos < serial_status_len)
			return serial_status_seq[serial_status_pos++];
		return serial_status_default;
	}
	if (port == STUB_PS2_STATUS)
	{
		return (keyboard_head < keyboard_tail) ? 0x01 : 0x00;
	}
	if (port == STUB_PS2_DATA)
	{
		if (keyboard_head < keyboard_tail)
			return keyboard_queue[keyboard_head++];
		return 0;
	}
	return 0;
}
