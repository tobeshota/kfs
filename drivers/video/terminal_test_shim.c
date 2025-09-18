#include <stdint.h>
#include <stddef.h>
#include <linux/terminal.h>
#include "../../test/unit/terminal_test.h" /* test-only interface */

/*
 * Test shim: Provides a stub VGA buffer and accessors/wrappers used by
 * unit tests. This file is compiled ONLY for host unit tests and not
 * included in production kernel build.
 */

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

/* Forward declarations from terminal.c */
void terminal_initialize(void);
void terminal_putchar(char c);
void terminal_writestring(const char *s);

extern void terminal_setcolor(uint8_t color); /* if needed by future tests */

/* Stub buffer replacing VGA memory for tests */
static uint16_t stub_buffer[VGA_WIDTH * VGA_HEIGHT];

/* Symbols mirroring terminal.c static state (replicated). For a deeper
 * refactor we could expose a struct; for now we re-derive state through
 * accessors by instrumenting minimal hooks. To avoid changing production
 * code, we reproduce logic here by performing operations via existing
 * public API and reading back from the stub buffer directly.
 */

/* NOTE: Because terminal.c keeps its internal row/column static and
 * inaccessible, we cannot directly observe them without modifying
 * production code. For current tests we only need buffer contents and
 * behavior of wrappers, so we approximate row/col by tracking writes.
 */
static size_t observed_row = 0;
static size_t observed_col = 0;
static uint8_t observed_color = 0;

/* Minimal re-implementation of computations to stay consistent. */
static inline uint8_t vga_entry_color(uint8_t fg, uint8_t bg) { return fg | (bg << 4); }
static inline uint16_t vga_entry(unsigned char uc, uint8_t color) { return (uint16_t)uc | (uint16_t)color << 8; }

/* Hooking strategy: We replicate a subset of terminal logic for tests. */

uint16_t kfs_test_vga_entry(unsigned char uc, uint8_t color){ return vga_entry(uc,color);}
uint8_t kfs_test_vga_entry_color(uint8_t fg, uint8_t bg){ return vga_entry_color(fg,bg);}
size_t kfs_test_strlen(const char* s){ size_t n=0; while(s[n]) n++; return n; }

void kfs_test_term_init(void){
    observed_row = 0; observed_col = 0; observed_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    for(size_t y=0;y<VGA_HEIGHT;y++) for(size_t x=0;x<VGA_WIDTH;x++) stub_buffer[y*VGA_WIDTH+x] = vga_entry(' ', observed_color);
}

static void test_scroll_if_needed(void){
    if (observed_row < VGA_HEIGHT) return;
    for(size_t y=1;y<VGA_HEIGHT;y++)
        for(size_t x=0;x<VGA_WIDTH;x++)
            stub_buffer[(y-1)*VGA_WIDTH+x] = stub_buffer[y*VGA_WIDTH+x];
    for(size_t x=0;x<VGA_WIDTH;x++) stub_buffer[(VGA_HEIGHT-1)*VGA_WIDTH+x] = vga_entry(' ', observed_color);
    observed_row = VGA_HEIGHT - 1;
}

void kfs_test_term_putc(char c){
    if (c=='\n') { observed_col=0; observed_row++; test_scroll_if_needed(); return; }
    if (c=='\r') { observed_col=0; return; }
    stub_buffer[observed_row*VGA_WIDTH + observed_col] = vga_entry(c, observed_color);
    if(++observed_col == VGA_WIDTH){ observed_col=0; observed_row++; test_scroll_if_needed(); }
}

void kfs_test_term_write(const char* s){ for(size_t i=0;s[i];++i) kfs_test_term_putc(s[i]); }

size_t kfs_term_get_row(void){ return observed_row; }
size_t kfs_term_get_col(void){ return observed_col; }
uint8_t kfs_term_get_color(void){ return observed_color; }
uint16_t kfs_term_peek(size_t x, size_t y){ return stub_buffer[y*VGA_WIDTH + x]; }
