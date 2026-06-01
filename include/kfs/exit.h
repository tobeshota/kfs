#ifndef _KFS_EXIT_H
#define _KFS_EXIT_H

struct task_struct;

typedef void (*exit_hook_t)(struct task_struct *tsk);

void register_exit_hook(exit_hook_t hook);
void invoke_exit_hooks(struct task_struct *tsk);

#endif /* _KFS_EXIT_H */
