/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mtk_events

#if !defined(_TRACE_MTK_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MTK_EVENTS_H

#include <linux/tracepoint.h>

TRACE_EVENT(tracing_on,

	TP_PROTO(int on, unsigned long ip),

	TP_ARGS(on, ip),

	TP_STRUCT__entry(
		__field(int, on)
		__field(unsigned long, ip)
	),

	TP_fast_assign(
		__entry->on = on;
		__entry->ip = ip;
	),

	TP_printk("ftrace is %s caller=%pf",
		__entry->on ? "enabled" : "disabled",
		(void *)__entry->ip)
);

TRACE_EVENT(cpu_hotplug,

	TP_PROTO(unsigned int cpu_id, unsigned int state,
		unsigned long long ts),

	TP_ARGS(cpu_id, state, ts),

	TP_STRUCT__entry(
		__field(u32, cpu_id)
		__field(u32, state)
		__field(u64, ts)
	),

	TP_fast_assign(
		__entry->cpu_id = cpu_id;
		__entry->state = state;
		__entry->ts = ts;
	),

	TP_printk("cpu=%03lu state=%s last_%s_ts=%llu",
		(unsigned long)__entry->cpu_id,
		__entry->state ? "online" : "offline",
		__entry->state ? "offline" : "online", __entry->ts
	)
);

TRACE_EVENT(irq_entry,

	TP_PROTO(int irqnr, const char *irqname),

	TP_ARGS(irqnr, irqname),

	TP_STRUCT__entry(
		__field(int, irq)
		__string(name, irqname)
	),

	TP_fast_assign(
		__entry->irq = irqnr;
		__assign_str(name, irqname);
	),

	TP_printk("irq=%d name=%s", __entry->irq, __get_str(name))
);

TRACE_EVENT(irq_exit,

	TP_PROTO(int irqnr),

	TP_ARGS(irqnr),

	TP_STRUCT__entry(
		__field(int, irq)
	),

	TP_fast_assign(
		__entry->irq = irqnr;
	),

	TP_printk("irq=%d", __entry->irq)
);

TRACE_EVENT(gpu_freq,

	TP_PROTO(unsigned int frequency),

	TP_ARGS(frequency),

	TP_STRUCT__entry(
		__field(u32, frequency)
	),

	TP_fast_assign(
		__entry->frequency = frequency;
	),

	TP_printk("frequency=%lu", (unsigned long)__entry->frequency)
);

TRACE_EVENT(ppm_update,

	TP_PROTO(unsigned int policy_mask,
		 unsigned int power_budget,
		 unsigned int root_cluster,
		 char *ppm_limits),

	TP_ARGS(policy_mask, power_budget, root_cluster, ppm_limits),

	TP_STRUCT__entry(
		__field(unsigned int, mask)
		__field(unsigned int, budget)
		__field(unsigned int, root)
		__string(limits, ppm_limits)
	),

	TP_fast_assign(
		__entry->mask = policy_mask;
		__entry->budget = power_budget;
		__entry->root = root_cluster;
		__assign_str(limits, ppm_limits);
	),

	TP_printk("(0x%x)(%d)(%d)%s", __entry->mask, __entry->budget,
		__entry->root, __get_str(limits))
);

TRACE_EVENT(ppm_user_setting,

		TP_PROTO(unsigned int policy_mask,
			int cid,
			unsigned int min_idx,
			unsigned int max_idx),

		TP_ARGS(policy_mask, cid, min_idx, max_idx),

		TP_STRUCT__entry(
			__field(unsigned int, mask)
			__field(int, cid)
			__field(unsigned int, min_idx)
			__field(unsigned int, max_idx)
			),

		TP_fast_assign(
			__entry->mask = policy_mask;
			__entry->cid = cid;
			__entry->min_idx = min_idx;
			__entry->max_idx = max_idx;
			),

		TP_printk("policy=%d cid=%d min=%d max=%d",
				__entry->mask, __entry->cid,
				__entry->min_idx, __entry->max_idx)
);

TRACE_EVENT(ppm_hica,

	TP_PROTO(const char *cur_state,
		 const char *target_state,
		 long usage,
		 long capacity,
		 int big_tsk_L,
		 int big_tsk_B,
		 int heavy_tsk,
		 int freq,
		 int result),

	TP_ARGS(cur_state, target_state, usage, capacity, big_tsk_L,
		big_tsk_B, heavy_tsk, freq, result),

	TP_STRUCT__entry(
		__string(cur, cur_state)
		__string(target, target_state)
		__field(long, usage)
		__field(long, capacity)
		__field(int, big_tsk_L)
		__field(int, big_tsk_B)
		__field(int, heavy_tsk)
		__field(int, freq)
		__field(int, result)
	),

	TP_fast_assign(
		__assign_str(cur, cur_state);
		__assign_str(target, target_state);
		__entry->usage = usage;
		__entry->capacity = capacity;
		__entry->big_tsk_L = big_tsk_L;
		__entry->big_tsk_B = big_tsk_B;
		__entry->heavy_tsk = heavy_tsk;
		__entry->freq = freq;
		__entry->result = result;
	),

	TP_printk
("%s->%s(%s), usage=%ld, capacity=%ld, big_tsk=%d/%d, heavy_tsk=%d, freq=%d",
		__get_str(cur), __get_str(target),
		(__entry->result) ? "O" : "X",
		__entry->usage, __entry->capacity,
		__entry->big_tsk_L, __entry->big_tsk_B,
		__entry->heavy_tsk, __entry->freq)
);

TRACE_EVENT(ppm_overutil_update,

	TP_PROTO(int overutil_l,
		 int overutil_h),

	TP_ARGS(overutil_l, overutil_h),

	TP_STRUCT__entry(
		__field(int, ou_l)
		__field(int, ou_h)
	),

	TP_fast_assign(
		__entry->ou_l = overutil_l;
		__entry->ou_h = overutil_h;
	),

	TP_printk("(%d)(%d)", __entry->ou_l, __entry->ou_h)
);

TRACE_EVENT(ppm_limit_callback_update,

	TP_PROTO(int i,
		 int has_advise_core,
		 int min_cpu_core,
		 int max_cpu_core),

	TP_ARGS(i, has_advise_core, min_cpu_core, max_cpu_core),

	TP_STRUCT__entry(
		__field(int, i)
		__field(int, has_advise_core)
		__field(int, min_cpu_core)
		__field(int, max_cpu_core)
	),

	TP_fast_assign(
		__entry->i = i;
		__entry->has_advise_core = has_advise_core;
		__entry->min_cpu_core = min_cpu_core;
		__entry->max_cpu_core = max_cpu_core;
	),

	TP_printk
	("ppm_limit_callback -> cluster%d: has_advise_core = %d, [%d, %d]\n",
		__entry->i, __entry->has_advise_core,
		__entry->min_cpu_core, __entry->max_cpu_core)
);

#if 0
TRACE_EVENT(sched_update,

	TP_PROTO(
		unsigned int cpu_id,
		unsigned int sched_info_cpu0,
		unsigned int sched_info_cpu1,
		unsigned int sched_info_cpu2,
		unsigned int sched_info_cpu3
	),

	TP_ARGS(cpu_id, sched_info_cpu0, sched_info_cpu1,
		sched_info_cpu2, sched_info_cpu3),

	TP_STRUCT__entry(
		__field(unsigned int, root_cpu)
		__field(unsigned int, sched_info_0)
		__field(unsigned int, sched_info_1)
		__field(unsigned int, sched_info_2)
		__field(unsigned int, sched_info_3)
	),

	TP_fast_assign(
		__entry->root_cpu = cpu_id;
		__entry->sched_info_0 = sched_info_cpu0;
		__entry->sched_info_1 = sched_info_cpu1;
		__entry->sched_info_2 = sched_info_cpu2;
		__entry->sched_info_3 = sched_info_cpu3;
	),

	TP_printk("(%d)(0x%x)(0x%x)(0x%x)(0x%x)",
	__entry->root_cpu, __entry->sched_info_0,
	__entry->sched_info_1, __entry->sched_info_2, __entry->sched_info_3)
);
#else
TRACE_EVENT(sched_update,

	TP_PROTO(
		unsigned int cluster,
		unsigned int sched_info_cluster
	),

	TP_ARGS(cluster, sched_info_cluster),

	TP_STRUCT__entry(
		__field(unsigned int, cluster_id)
		__field(unsigned int, sched_info)
	),

	TP_fast_assign(
		__entry->cluster_id = cluster;
		__entry->sched_info = sched_info_cluster;
	),

	TP_printk("(%d)(0x%x)", __entry->cluster_id, __entry->sched_info)
);
#endif

TRACE_EVENT(sspm_ipi,

	TP_PROTO(
		int start,
		int ipi_id,
		int ipi_opt
	),

	TP_ARGS(start, ipi_id, ipi_opt),

	TP_STRUCT__entry(
		__field(int, start)
		__field(int, ipi_id)
		__field(int, ipi_opt)
	),

	TP_fast_assign(
		__entry->start = start;
		__entry->ipi_id = ipi_id;
		__entry->ipi_opt = ipi_opt;
	),

	TP_printk("start=%d, id=%d, opt=%d",
	__entry->start, __entry->ipi_id, __entry->ipi_opt)
);

TRACE_EVENT(trigger_lowmem_hint,
	TP_PROTO(
		long mm_free,
		long mm_thrash
	),

	TP_ARGS(mm_free, mm_thrash),

	TP_STRUCT__entry(
		__field(long, mm_free)
		__field(long, mm_thrash)
	),

	TP_fast_assign(
		__entry->mm_free = mm_free;
		__entry->mm_thrash = mm_thrash;
	),

	TP_printk("free=%ld thrash=%ld",
		__entry->mm_free, __entry->mm_thrash
	)
);

TRACE_EVENT(lowmem_hint_uevent,
	TP_PROTO(
		int ret
	),

	TP_ARGS(ret),

	TP_STRUCT__entry(
		__field(int, ret)
	),

	TP_fast_assign(
		__entry->ret = ret;
	),

	TP_printk("ret=%d",
		__entry->ret
	)
);

TRACE_EVENT(perf_index_s,
	TP_PROTO(
		unsigned int sf0,
		unsigned int sf1,
		unsigned int sf2,
		int dram_freq,
		int bw_c,
		int bw_g,
		int bw_mm,
		int bw_total,
		int vcore_uv
	),

	TP_ARGS(sf0, sf1, sf2, dram_freq, bw_c, bw_g, bw_mm, bw_total,
		vcore_uv),

	TP_STRUCT__entry(
		__field(unsigned int, sf0)
		__field(unsigned int, sf1)
		__field(unsigned int, sf2)
		__field(int, dram_freq)
		__field(int, bw_c)
		__field(int, bw_g)
		__field(int, bw_mm)
		__field(int, bw_total)
		__field(int, vcore_uv)
	),

	TP_fast_assign(
		__entry->sf0       = sf0;
		__entry->sf1       = sf1;
		__entry->sf2       = sf2;
		__entry->dram_freq = dram_freq;
		__entry->bw_c      = bw_c;
		__entry->bw_g      = bw_g;
		__entry->bw_mm     = bw_mm;
		__entry->bw_total  = bw_total;
		__entry->vcore_uv  = vcore_uv;
	),

	TP_printk("sched_freq=%d|%d|%d dram_freq=%d bw=%d|%d|%d|%d vcore=%d",
		__entry->sf0,
		__entry->sf1,
		__entry->sf2,
		__entry->dram_freq,
		__entry->bw_c,
		__entry->bw_g,
		__entry->bw_mm,
		__entry->bw_total,
		__entry->vcore_uv)
);

#endif /* _TRACE_MTK_EVENTS_H */

TRACE_EVENT(swpm_power,

	TP_PROTO(char *power),

	TP_ARGS(power),

	TP_STRUCT__entry(
		 __string(power_str, power)
	),

	TP_fast_assign(
		__assign_str(power_str, power);
	),

	TP_printk("%s", __get_str(power_str))
);

/* This part must be outside protection */
#include <trace/define_trace.h>
