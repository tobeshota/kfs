#ifndef _KFS_PCSPKR_H
#define _KFS_PCSPKR_H

/*
 * i8254 PIT PC Speaker Driver
 */

#include <kfs/stdint.h>

void pcspkr_init(void);
void pcspkr_tone(uint32_t freq_hz);
void pcspkr_stop(void);

#endif /* _KFS_PCSPKR_H */
