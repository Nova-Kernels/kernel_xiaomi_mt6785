/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM sched

#if !defined(_TRACE_HOOK_SCHED_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_SCHED_H

#include <linux/tracepoint.h>
#include <trace/hooks/vendor_hooks.h>

/*
 * Now, we can define the vendor hooks.
 */
DECLARE_RESTRICTED_HOOK(android_vh_sched_wakeup_pre,
	TP_PROTO(struct task_struct *p),
	TP_ARGS(p),
	TP_CONDITION(1)
);

DECLARE_RESTRICTED_HOOK(android_vh_sched_wakeup_post,
	TP_PROTO(struct task_struct *p, int cpu),
	TP_ARGS(p, cpu),
	TP_CONDITION(1)
);

DECLARE_RESTRICTED_HOOK(android_vh_sched_update_load_avg_pre,
	TP_PROTO(struct task_struct *p, struct sched_avg *sa,
		 int cpu, unsigned long now),
	TP_ARGS(p, sa, cpu, now),
	TP_CONDITION(1)
);

DECLARE_RESTRICTED_HOOK(android_vh_sched_update_load_avg_post,
	TP_PROTO(struct task_struct *p, unsigned long contrib,
		 struct cfs_rq *cfs_rq),
	TP_ARGS(p, contrib, cfs_rq),
	TP_CONDITION(1)
);

#endif /* _TRACE_HOOK_SCHED_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
