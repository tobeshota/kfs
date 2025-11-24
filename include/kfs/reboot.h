#ifndef KFS_REBOOT_H
#define KFS_REBOOT_H

void machine_restart_kbd(void) __attribute__((weak));
void machine_restart(void) __attribute__((noreturn));

#endif /* KFS_REBOOT_H */
