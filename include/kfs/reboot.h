#ifndef KFS_REBOOT_H
#define KFS_REBOOT_H

/**
 * machine_restart - システムを再起動する
 *
 * この関数は戻らない。
 * Linux 2.6.11 arch/i386/kernel/reboot.c::machine_restart()を参考
 */
void machine_restart(void) __attribute__((noreturn));

#endif /* KFS_REBOOT_H */
