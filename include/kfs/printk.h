#ifndef KFS_PRINTK_H
#define KFS_PRINTK_H

#include <stdarg.h>
#include <stddef.h>

/* Loglevel escape prefixes (Linux inspired) */
#define KFS_KERN_SOH "\001"
#define KERN_SOH KFS_KERN_SOH
#define KERN_EMERG KFS_KERN_SOH "0"
#define KERN_ALERT KFS_KERN_SOH "1"
#define KERN_CRIT KFS_KERN_SOH "2"
#define KERN_ERR KFS_KERN_SOH "3"
#define KERN_WARNING KFS_KERN_SOH "4"
#define KERN_NOTICE KFS_KERN_SOH "5"
#define KERN_INFO KFS_KERN_SOH "6"
#define KERN_DEBUG KFS_KERN_SOH "7"
#define KERN_DEFAULT KFS_KERN_SOH "d"
#define KERN_CONT KFS_KERN_SOH "c"

/* Internal loglevel indices */
enum kfs_printk_loglevel
{
	KFS_LOGLEVEL_EMERG = 0,
	KFS_LOGLEVEL_ALERT = 1,
	KFS_LOGLEVEL_CRIT = 2,
	KFS_LOGLEVEL_ERR = 3,
	KFS_LOGLEVEL_WARNING = 4,
	KFS_LOGLEVEL_NOTICE = 5,
	KFS_LOGLEVEL_INFO = 6,
	KFS_LOGLEVEL_DEBUG = 7,
	KFS_LOGLEVEL_CONT = 8,
	KFS_LOGLEVEL_DEFAULT = KFS_LOGLEVEL_WARNING,
};

extern int console_printk[4];
#define console_loglevel (console_printk[0])
#define default_message_loglevel (console_printk[1])
#define minimum_console_loglevel (console_printk[2])
#define default_console_loglevel (console_printk[3])

int printk(const char *fmt, ...);
int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
int snprintf(char *buf, size_t size, const char *fmt, ...);

void kfs_printk_set_console_loglevel(int level);
int kfs_printk_get_console_loglevel(void);
int kfs_printk_get_default_loglevel(void);

#endif /* KFS_PRINTK_H */
