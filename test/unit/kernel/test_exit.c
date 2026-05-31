#include "../test_reset.h"
#include "unit_test_framework.h"
#include <kfs/gfp.h>
#include <kfs/list.h>
#include <kfs/mm.h>
#include <kfs/pid.h>
#include <kfs/sched.h>
#include <kfs/slab.h>
#include <kfs/wait.h>

/**
 * test_exit.c — do_exit / sys_exit / release_task の単体テスト
 *
 * 【テスト方針】
 * do_exit() は noreturn であり、呼び出し後に状態を確認することはできない。
 * そのため、本番環境と同様に kernel_thread() + do_wait() を使い、
 * 親プロセス（init_task）の視点から observable な動作を検証する。
 *
 * kernel_thread(fn) はカーネル関数を ring-0 で実行するカーネルスレッドを作成する
 * （copy_thread_with_fn → fork_frame.ebx=fn → ret_from_fork が call *%%ebx）。
 * do_fork(user_eip, user_esp) は ring-3 iret 経由で起動するため、カーネル関数を
 * 渡すと ring-3 のまま sys_exit/schedule 等を呼び出して ESP が崩壊するので使えない。
 */

/* テスト対象関数（kernel/exit.c） */
extern void release_task(struct task_struct *p);
extern void sys_exit(int error_code);

/* テスト用ヘルパー（kernel/sched/core.c） */
extern struct task_struct *current;
extern struct task_struct init_task;
extern struct list_head task_list;

/* 初期化関数 */
extern void fork_init(void);
extern pid_t kernel_thread(void (*fn)(void), const char *name);
extern pid_t do_wait(int *wstatus, int options);
extern void pid_init(void);
extern void init_idle_task(void);

/* 全テストで共通のセットアップ */
static void setup_test(void)
{
	reset_all_state_for_test();
	kmem_cache_init();
	pid_init();
	init_idle_task();
	fork_init();
}

static void teardown_test(void)
{
}

/* ---- 子プロセス用ヘルパー関数 ---- */

static void fn_exit_42(void)
{
	sys_exit(42);
}

static void fn_exit_0(void)
{
	sys_exit(0);
}

static void fn_exit_255(void)
{
	sys_exit(255);
}

/* mm テスト用: init_task.mm をコピーした状態で exit する */
static void fn_exit_with_mm(void)
{
	sys_exit(0);
}

/* reparent テスト用: 孫を kernel_thread で起動してすぐ自分は exit */
static void fn_parent_reparent(void)
{
	kernel_thread(fn_exit_0, NULL);
	sys_exit(0);
}

/**
 * test_do_exit_basic:
 * 子が sys_exit(42) で終了し、do_wait が正しい PID を返すこと。
 * wstatus に POSIX 形式（42 << 8）の終了コードが格納されること。
 */
KFS_TEST(test_do_exit_basic)
{
	int wstatus = 0;
	pid_t child_pid, waited_pid;

	child_pid = kernel_thread(fn_exit_42, NULL);
	KFS_ASSERT_TRUE(child_pid > 0);

	waited_pid = do_wait(&wstatus, 0);
	KFS_ASSERT_EQ((int)waited_pid, (int)child_pid);
	KFS_ASSERT_EQ(wstatus, 42 << 8);

	printk("do_exit basic test passed\n");
}

/**
 * test_do_exit_exit_code_zero:
 * 終了コード 0 が正しく wstatus に返ること。
 */
KFS_TEST(test_do_exit_exit_code_zero)
{
	int wstatus = -1;
	pid_t child_pid;

	child_pid = kernel_thread(fn_exit_0, NULL);
	KFS_ASSERT_TRUE(child_pid > 0);

	pid_t waited = do_wait(&wstatus, 0);
	KFS_ASSERT_EQ((int)waited, (int)child_pid);
	KFS_ASSERT_EQ(wstatus, 0);

	printk("do_exit exit code zero test passed\n");
}

/**
 * test_sys_exit:
 * sys_exit(255) が終了コードを POSIX 形式（(255 & 0xff) << 8）で格納すること。
 */
KFS_TEST(test_sys_exit)
{
	int wstatus = 0;
	pid_t child_pid;

	child_pid = kernel_thread(fn_exit_255, NULL);
	KFS_ASSERT_TRUE(child_pid > 0);

	pid_t waited = do_wait(&wstatus, 0);
	KFS_ASSERT_EQ((int)waited, (int)child_pid);
	KFS_ASSERT_EQ(wstatus, (255 & 0xff) << 8);

	printk("sys_exit test passed\n");
}

/**
 * test_do_exit_mm_free:
 * mm を持つ子プロセスが exit しても release_task がクラッシュしないこと。
 * （do_exit 内で mm が解放済みのため、release_task が二重解放しないことを確認）
 */
KFS_TEST(test_do_exit_mm_free)
{
	static struct mm_struct test_mm;
	pid_t child_pid;

	/* init_task に mm を設定 → copy_process で子に複製される */
	test_mm.mm_count.counter = 1;
	test_mm.pgd = NULL;
	test_mm.brk = 0x08048000;
	test_mm.start_stack = 0x08049000;
	init_task.mm = &test_mm;

	child_pid = kernel_thread(fn_exit_with_mm, NULL);
	init_task.mm = NULL; /* 親の mm を元に戻す */
	KFS_ASSERT_TRUE(child_pid > 0);

	/* クラッシュせずに子を回収できること */
	pid_t waited = do_wait(NULL, 0);
	KFS_ASSERT_EQ((int)waited, (int)child_pid);

	printk("do_exit mm free test passed\n");
}

/**
 * test_release_task_basic:
 * do_wait 後に子が task_list から除去されていること（release_task の動作確認）。
 */
KFS_TEST(test_release_task_basic)
{
	pid_t child_pid;
	struct list_head *pos;
	int count;

	child_pid = kernel_thread(fn_exit_0, NULL);
	KFS_ASSERT_TRUE(child_pid > 0);
	KFS_ASSERT_TRUE(!list_empty(&task_list)); /* kernel_thread 後はタスクリストにある */

	do_wait(NULL, 0); /* 内部で release_task が呼ばれる */

	/* init_task のみが残ること */
	count = 0;
	list_for_each(pos, &task_list) count++;
	KFS_ASSERT_EQ(count, 1);

	printk("release_task basic test passed\n");
}

/**
 * test_do_exit_reparent_children:
 * 親が exit したとき、孫が init_task の子リストに移動すること。
 *
 * シナリオ:
 *   init_task
 *     └─ parent (fn_parent_reparent)
 *           └─ grandchild (fn_exit_0)
 *
 * parent が exit すると grandchild は init_task に reparent される。
 * init_task は parent を do_wait で回収後、grandchild も回収できる。
 */
KFS_TEST(test_do_exit_reparent_children)
{
	pid_t parent_pid, waited;

	parent_pid = kernel_thread(fn_parent_reparent, NULL);
	KFS_ASSERT_TRUE(parent_pid > 0);

	/* parent を回収（parent の do_exit 内で grandchild が init_task に reparent） */
	waited = do_wait(NULL, 0);
	KFS_ASSERT_EQ((int)waited, (int)parent_pid);

	/* grandchild が init_task の children に残っているはず */
	KFS_ASSERT_TRUE(!list_empty(&init_task.children));

	/* grandchild も回収できること */
	do_wait(NULL, 0);
	KFS_ASSERT_TRUE(list_empty(&init_task.children));

	printk("do_exit reparent children test passed\n");
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_do_exit_basic, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_exit_exit_code_zero, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_exit, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_exit_mm_free, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_release_task_basic, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_exit_reparent_children, setup_test, teardown_test),
};

int register_unit_tests_exit(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
