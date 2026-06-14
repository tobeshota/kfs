#include <kfs/fair.h>
#include <kfs/rbtree.h>
#include <kfs/sched.h>
#include <kfs/stddef.h>

#define FAIR_VRUNTIME_TICK NICE_0_LOAD /* 1 tick あたりの vruntime 増分 */

/** CFS (Completely Fair Scheduling) runqueue
 * @brief 実行可能な fair task を vruntime 順の二分木で管理する
 */
struct cfs_rq
{
	struct rb_root tasks_timeline; /* 実行可能な task を vruntime 順に並べる赤黒木 */
	struct rb_node *rb_leftmost; /* vruntime が最小の node．つまり次に動かすべきtask（キャッシュ用途） */
	unsigned int nr_running; /* fair runqueue 上の task 数 */
	uint64_t min_vruntime;	 /* tasks_timeline 内の最小 vruntime．rb_leftmost の vruntime と等しい */
};

static struct cfs_rq cfs_rq;

/* nice値に対応する，CFSのvruntime計算に使用される重み */
static const unsigned long sched_prio_to_weight[NICE_WIDTH] = {
	88761, 71755, 56483, 46273, 36291, 29154, 23254, 18705, 14949, 11916, 9548, 7620, 6100, 4904,
	3906,  3121,  2501,	 1991,	1586,  1277,  1024,	 820,	655,   526,	  423,	335,  272,	215,
	172,   137,	  110,	 87,	70,	   56,	  45,	 36,	29,	   23,	  18,	15,
};

/** nice 値に対応する CFSの重みを返す
 * @param nice nice 値
 * @return nice に対応する重み
 */
unsigned long sched_weight_for_nice(int nice)
{
	/* 範囲外の値は Linux と同様に端へ丸める */
	if (nice < NICE_MIN)
	{
		nice = NICE_MIN;
	}
	else if (nice > NICE_MAX)
	{
		nice = NICE_MAX;
	}

	return sched_prio_to_weight[nice + NICE_0_INDEX];
}

/** task の scheduling entity を runqueue 外状態へ初期化する
 * @param task 初期化する task
 * @note vruntime は fork 時の親からの継承を保つため変更しない
 */
void sched_init_entity(struct task_struct *task)
{
	task->se.load = sched_weight_for_nice(task->nice);
	task->se.run_node.__rb_parent_color = 0;
	task->se.run_node.rb_left = NULL;
	task->se.run_node.rb_right = NULL;
	task->se.on_rq = 0;
}

/** task の load weight が未初期化なら nice 値から設定する
 * @param task 対象 task
 */
static void fair_ensure_load(struct task_struct *task)
{
	if (task->se.load == 0)
	{
		task->se.load = sched_weight_for_nice(task->nice);
	}
}

/** 1 tick 分の実行時間を vruntime に換算する
 * @param weight task の load weight
 * @return weight を反映した vruntime 増分
 */
static uint64_t fair_delta_vruntime(unsigned long weight)
{
	if (weight == 0)
	{
		weight = NICE_0_LOAD;
	}
	return (uint64_t)FAIR_VRUNTIME_TICK * NICE_0_LOAD / weight;
}

/** rb_node から task_struct を取得する
 * @param node task->se.run_node へのポインタ
 * @return node を含む task_struct
 */
static struct task_struct *task_of_node(struct rb_node *node)
{
	return rb_entry(node, struct task_struct, se.run_node);
}

/** fair runqueue の左端 task を返す
 * @return 最小 vruntime の task。runqueue が空なら NULL
 */
static struct task_struct *fair_leftmost_task(void)
{
	/* rb_leftmost が NULL の場合 runqueue は空 */
	if (!cfs_rq.rb_leftmost)
	{
		return NULL;
	}

	return task_of_node(cfs_rq.rb_leftmost);
}

/** root内でoldノードをnewノードに置き換える
 * @param old 置き換え元 node
 * @param new 置き換え先 node．NULL 可
 */
static void fair_rb_transplant(struct rb_node *old, struct rb_node *new)
{
	struct rb_node *parent = rb_parent(old);

	if (!parent)
	{
		/* oldノードが root の場合，
		 * root を newノードに置き換える */
		cfs_rq.tasks_timeline.rb_node = new;
	}
	else if (parent->rb_left == old)
	{
		/* oldノードが親の左子の場合
		 * 親の左子を newノードに置き換える */
		parent->rb_left = new;
	}
	else
	{
		/* oldノードが親の右子の場合
		 * 親の右子を newノードに置き換える */
		parent->rb_right = new;
	}

	/* newノードが NULL でない場合は親を設定する */
	if (new)
	{
		rb_set_parent(new, parent);
	}
}

/** 部分木の最小 node を返す
 * @param node 探索を開始する node
 * @return node 以下の最小 node
 */
static struct rb_node *fair_rb_minimum(struct rb_node *node)
{
	/* 左子が存在する限り左へ進むことで
	 * node 以下の最小 node を探索する */
	while (node && node->rb_left)
	{
		node = node->rb_left;
	}

	return node;
}

/** fair runqueue から node を削除する
 * @param node 削除する node
 * @note 既存 rb_erase() はまだ簡易実装なので、CFS runqueue に必要な二分木削除をここで行う。
 */
static void fair_erase_node(struct rb_node *node)
{
	if (!node->rb_left)
	{
		/** node の左子が存在しない場合，右子で置き換える
		 * @details 図を用いた例:
		 * node が削除対象で node->rb_right が存在する場合
		 *
		 * 	 parent
		 * 	 /     \
		 *   node    sibling
		 *     \
		 *    node->rb_right
		 *
		 * node を削除して，node->rb_right を node の位置に移動させる
		 *
		 * 	 parent
		 * 	 /     \
		 * node->rb_right sibling
		 */
		fair_rb_transplant(node, node->rb_right);
	}
	else if (!node->rb_right)
	{
		/** node の右子が存在しない場合，左子で置き換える
		 * @details 図を用いた例:
		 * node が削除対象で node->rb_left が存在する場合
		 * 	 parent
		 * 	 /     \
		 *   node    sibling
		 *   /
		 * node->rb_left
		 *
		 * node を削除して，node->rb_left を node の位置に移動させる
		 *
		 * 	 parent
		 * 	 /     \
		 * node->rb_left sibling
		 */
		fair_rb_transplant(node, node->rb_left);
	}
	else
	{
		/** node の左右子が存在する場合，node の後継ノードで置き換える
		 * @details 図を用いた例:
		 * node が削除対象で node->rb_left, node->rb_right が存在する場合
		 *
		 * 	  parent
		 * 	  /        \
		 *   node(値20)  sibling
		 *   /        \
		 *  A(値10)    B(値40)
		 *    /
		 *   C(値30)
		 *
		 * node を削除して，node の後継ノードを node の位置に移動させる．
		 * ここで node の後継ノードとは，削除したい node の右部分木の中でいちばん小さい node，
		 * すなわち node の右子の部分木の最小ノードであり，この例では C である．
		 *
		 * 	  parent
		 * 	  /     \
		 *   C(値30)  sibling
		 *   /      \
		 *  A(値10)  B(値40)
		 */

		struct rb_node *successor = fair_rb_minimum(node->rb_right); /* node の後継ノード */

		/* successor が node の直下にいない場合 */
		if (rb_parent(successor) != node)
		{
			/* successor が node の直下にいない場合，
			 * successor を node の位置に移動させる前に
			 * successor の右子を successor の位置に移動させる */
			fair_rb_transplant(successor, successor->rb_right); /* successor の右子を successor の位置に移動させる */
			successor->rb_right = node->rb_right;		   /* node の右子を successor の右子に設定する */
			rb_set_parent(successor->rb_right, successor); /* successor を親として設定する */
		}

		/* node を successor で置き換える */
		fair_rb_transplant(node, successor);		  /* node を successor で置き換える */
		successor->rb_left = node->rb_left;			  /* node の左子を successor の左子に設定する */
		rb_set_parent(successor->rb_left, successor); /* successor を親として設定する */
	}

	/* node を初期化する */
	node->__rb_parent_color = 0;
	node->rb_left = NULL;
	node->rb_right = NULL;
}

/** rb_leftmost と min_vruntime を更新する
 * @note rb_leftmost は fair runqueue のキャッシュ済み左端
 * @note min_vruntime は fair runqueue 内の最小 vruntimeであり，
 *       rb_leftmost の vruntime と等しい
 */
static void fair_update_leftmost(void)
{
	/* rb_leftmost を更新する */
	cfs_rq.rb_leftmost = rb_first(&cfs_rq.tasks_timeline);

	/* min_vruntime を更新する */
	struct task_struct *leftmost = fair_leftmost_task();
	if (leftmost)
	{
		cfs_rq.min_vruntime = leftmost->se.vruntime;
	}
}

/* fair scheduler class を初期化する */
static void fair_init(void)
{
	cfs_rq.tasks_timeline = RB_ROOT;
	cfs_rq.rb_leftmost = NULL;
	cfs_rq.nr_running = 0;
	cfs_rq.min_vruntime = 0;
}

/** task を vruntime 順で fair runqueue に追加する
 * @param task enqueue する task
 * @example
 * 挿入例: 既に vruntime 順に 10, 20, 30 のタスクが存在している状態で，
 *        vruntime=25 のタスクを追加する場合
 * 1. 挿入位置の探索
 *    - ルートから開始して，25 < 20 で右へ，次に 25 < 30 で左へ進むため，
 *      20 の右子が挿入位置となる
 *       20      ルートから開始する
 *      /  \     25 < 20 で右へ進む
 *    10    30
 *          /    25 < 30 で左へ進む
 *        25
 * 2. ノードの挿入
 *    - 25 のタスクを 20 の右子として挿入する
 * 3. rb_insert_color による再構成
 *    - 25 のタスクは赤色で挿入されるため，親の 20 が黒色であれば，再構成は不要
 *    - もし親の 20 が赤色であれば，再構成が必要となる
 * 4. rb_leftmost と min_vruntime の更新
 *    - 25 は 30 より小さいため，rb_leftmost は 25 に更新され，
 *      min_vruntime も 25 に更新される
 */
static void fair_enqueue_task(struct task_struct *task)
{
	/* すでに runqueue に載っている場合は何もしない */
	if (task->se.on_rq)
	{
		return;
	}

	/* task の load を更新する */
	fair_ensure_load(task);

	struct rb_node **link = &cfs_rq.tasks_timeline.rb_node; /* 挿入位置のリンク */
	struct rb_node *parent = NULL;							/* 挿入位置の親ノード */
	struct task_struct *entry;								/* 挿入位置のノードから取得した task_struct */
	int leftmost = 1;										/* 挿入位置が左端かどうかを示すフラグ */

	/* 挿入位置のリンクを探索する */
	while (*link)
	{
		parent = *link;
		entry = task_of_node(parent);

		/* task の vruntime が entry の vruntime より小さいか？
		 * または vruntime が同じで pid が小さいか？ */
		if (task->se.vruntime < entry->se.vruntime ||
			(task->se.vruntime == entry->se.vruntime && task->pid < entry->pid))
		{
			/* 挿入位置が左端の場合 */
			link = &parent->rb_left;
		}
		else
		{
			/* 挿入位置が左端でない場合 */
			leftmost = 0;
			link = &parent->rb_right;
		}
	}

	/* ノードをリンクする */
	rb_link_node(&task->se.run_node, parent, link);
	rb_insert_color(&task->se.run_node, &cfs_rq.tasks_timeline);

	if (leftmost)
	{
		cfs_rq.rb_leftmost = &task->se.run_node;
		cfs_rq.min_vruntime = task->se.vruntime;
	}
	cfs_rq.nr_running++;
	task->se.on_rq = 1;
}

/** task を fair runqueue から削除する
 * @param task dequeue する task
 */
static void fair_dequeue_task(struct task_struct *task)
{
	/* runqueue に載っていない場合は何もしない */
	if (!task->se.on_rq)
	{
		return;
	}

	fair_erase_node(&task->se.run_node);
	task->se.on_rq = 0;
	if (cfs_rq.nr_running > 0)
	{
		cfs_rq.nr_running--;
	}
	fair_update_leftmost();
}

/** task が fair runqueue に載っているかを返す
 * @param task 確認する task
 * @return 1=runqueue 上, 0=runqueue 外
 */
static int fair_task_queued(struct task_struct *task)
{
	return task->se.on_rq != 0;
}

/** 次に実行する fair task を返す
 * @return 最小 vruntime の task。存在しない場合は NULL
 */
static struct task_struct *fair_pick_next_task(void)
{
	return fair_leftmost_task();
}

/** fair task の tick 処理
 * @param task 現在実行中の task
 * @details 実行済み tick を nice weight に応じた vruntime に換算し、
 *          runqueue 上の task であれば tree の順序を保つため挿し直す。
 */
static void fair_task_tick(struct task_struct *task)
{
	/* task の load を更新する */
	fair_ensure_load(task);

	const int queued = task->se.on_rq != 0; /* タスクが runqueue に載っているか */

	if (queued)
	{
		/* タスクが runqueue に載っている場合，
		 * runqueue から削除する */
		fair_dequeue_task(task);
	}

	/* 実行済み tick を vruntime に換算する */
	task->se.vruntime += fair_delta_vruntime(task->se.load);

	if (queued)
	{
		/* タスクが runqueue に載っている場合，
		 * runqueue に再追加する */
		fair_enqueue_task(task);
	}
}

/* 通常プロセス用 fair scheduler class */
const struct sched_class fair_sched_class = {
	.init = fair_init,
	.enqueue_task = fair_enqueue_task,
	.dequeue_task = fair_dequeue_task,
	.task_queued = fair_task_queued,
	.pick_next_task = fair_pick_next_task,
	.task_tick = fair_task_tick,
};
