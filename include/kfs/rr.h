#ifndef _KFS_RR_H
#define _KFS_RR_H

struct task_struct; /* 前方宣言 */

/** SCHED_PURE_RRのタイムスライスのデフォルト値（単位: ティック数）
 * @note HZ=1000(1ティック=1ms) のとき 10ms に相当する
 */
#define RR_TIMESLICE 10

void rr_init(void);
void rr_enqueue(struct task_struct *tsk);
void rr_dequeue(struct task_struct *tsk);
struct task_struct *rr_pick_next(void);
void rr_task_tick(struct task_struct *tsk);

#endif /* _KFS_RR_H */
