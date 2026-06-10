#ifndef _TTY_H
#define _TTY_H

#include <kfs/stddef.h>
#include <kfs/termios.h>

void tty_reset(void);
long tty_read_line_for_console(size_t console_index, char *buf, unsigned int size);
void tty_input_char_for_console(size_t console_index, char ch);
void tty_handle_backspace_for_console(size_t console_index);
void tty_handle_enter_for_console(size_t console_index);
void tty_handle_cursor_left_for_console(size_t console_index);
void tty_handle_cursor_right_for_console(size_t console_index);
void tty_discard_input_for_console(size_t console_index, int publish_empty);
int tty_set_echo_for_console(size_t console_index, int enabled);
int tty_get_echo_for_console(size_t console_index);
int tty_get_termios_for_console(size_t console_index, struct termios *termios);
int tty_set_termios_for_console(size_t console_index, const struct termios *termios);
int tty_signal_enabled_for_console(size_t console_index);

#endif /* _TTY_H */
