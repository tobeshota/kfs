#ifndef _KFS_ERRNO_H
#define _KFS_ERRNO_H

#define EPERM 1			   /* Operation not permitted */
#define EINTR 4			   /* Interrupted system call */
#define ECHILD 10		   /* No child processes */
#define EAGAIN 11		   /* Try again (リソース一時的に利用不可) */
#define ENOMEM 12		   /* Out of memory */
#define EINVAL 22		   /* Invalid argument */
#define ENOTTY 25		   /* Inappropriate ioctl for device */
#define ESRCH 3			   /* No such process */
#define EBADF 9			   /* Bad file number */
#define ENOSYS 38		   /* Function not implemented */
#define EPROTONOSUPPORT 93 /* Protocol not supported */
#define EAFNOSUPPORT 97	   /* Address family not supported */

#endif /* _KFS_ERRNO_H */
