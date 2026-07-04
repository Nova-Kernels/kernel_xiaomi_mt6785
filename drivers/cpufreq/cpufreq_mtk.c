/*
 * drivers/cpufreq/cpufreq_mtk.c
 *
 * Copyright (C) 2022 bengris32
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/cpufreq.h>
#include <linux/sched.h>
#include <linux/arch_topology.h>
#include <cpu_ctrl.h>
#include <sched_ctl.h>

#define CLUSTER_NUM 2
#define LITTLE 0
#define BIG 1

#define cpufreq_mtk_attr(_name)						\
static struct kobj_attribute _name##_attr =			\
__ATTR(_name, 0644, show_##_name, store_##_name)

/* cpu frequency table from cpufreq dt parse */
static struct cpufreq_frequency_table* cpuftbl[2];

static struct ppm_limit_data *current_cpu_freq;

DEFINE_MUTEX(cpufreq_mtk_mutex);

struct cpufreq_mtk_topo_config {
    unsigned int ltl_cpu_start;
    unsigned int big_cpu_start;
};

#if defined(CONFIG_MACH_MT6785)
static const struct cpufreq_mtk_topo_config topology = {
    .ltl_cpu_start			= 0,
    .big_cpu_start			= 6,
};
#endif

void cpufreq_mtk_set_table(int cpu, struct cpufreq_frequency_table *ftbl)
{
	if ( cpu == topology.big_cpu_start )
		cpuftbl[BIG] = ftbl;
	else if ( cpu == topology.ltl_cpu_start )
		cpuftbl[LITTLE] = ftbl;
}
EXPORT_SYMBOL_GPL(cpufreq_mtk_set_table);

int is_freq_valid(int cluster, int freq) {
    struct cpufreq_frequency_table *pos;

    /*
     * Allow -1 frequency as that is
     * used to remove the limit.
     */
    if (freq == -1)
        return 1;

    cpufreq_for_each_valid_entry(pos, cpuftbl[cluster]) {
        if (pos->frequency == freq)
            return 1;
    }

    return 0;
}

/*
 * Scale a cluster's min-freq request into uclamp (0..SCHED_CAPACITY_SCALE)
 * space using that cluster's own capacity/max freq, as the energy model does.
 */
static unsigned int cluster_uclamp_floor(int cluster)
{
    unsigned int cpu, cap, max_freq;

    if (current_cpu_freq[cluster].min <= 0)
        return 0;

    cpu = (cluster == BIG) ? topology.big_cpu_start : topology.ltl_cpu_start;
    cap = topology_get_cpu_scale(NULL, cpu);
    max_freq = topology_get_max_cpu_freq(NULL, cpu);

    if (!cap || !max_freq)
        return 0;

    return mult_frac(cap, current_cpu_freq[cluster].min, max_freq);
}

/*
 * Raise the uclamp_min floor to the strongest active min-freq request so EAS
 * keeps satisfying it. A max-freq ceiling (thermal/battery) must not raise it.
 */
static void update_cpu_freq_boost(void)
{
    int i;
    unsigned int floor = 0;

    for (i = 0; i < CLUSTER_NUM; i++) {
        unsigned int req_floor = cluster_uclamp_floor(i);

        if (req_floor > floor)
            floor = req_floor;
    }

    if (floor > SCHED_CAPACITY_SCALE)
        floor = SCHED_CAPACITY_SCALE;

    sched_set_uclamp_min_floor(floor);
}

/* Updates CPU frequency for chosen cluster */
void update_cpu_freq(int cluster)
{
    update_cpu_freq_boost();
    update_userlimit_cpu_freq(CPU_KIR_PERF, CLUSTER_NUM, current_cpu_freq);
}

/* Sets current maximum CPU frequency */
int set_max_cpu_freq(int cluster, int max)
{
    int ret = -EINVAL;

    if (unlikely(!is_freq_valid(cluster, max)))
        goto out;

    if (unlikely(max < current_cpu_freq[cluster].min && current_cpu_freq[cluster].min > 0))
        goto out;

    current_cpu_freq[cluster].max = max > 0 ? max : -1;
    update_cpu_freq(cluster);
    ret = 0;

out:
    return ret;
}

/* Sets current minimum CPU frequency */
int set_min_cpu_freq(int cluster, int min)
{
    int ret = -EINVAL;

    if (unlikely(!is_freq_valid(cluster, min)))
        goto out;

    if (unlikely(min > current_cpu_freq[cluster].max && current_cpu_freq[cluster].max > 0))
        goto out;

    current_cpu_freq[cluster].min = min > 0 ? min : -1;
    update_cpu_freq(cluster);
    ret = 0;

out:
    return ret;
}

static ssize_t show_lcluster_min_freq(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", current_cpu_freq[LITTLE].min);
}

static ssize_t store_lcluster_min_freq(struct kobject *kobj,
                    struct kobj_attribute *attr, const char *buf,
                    size_t count)
{
    int ret, new_freq;

    ret = sscanf(buf, "%d", &new_freq);
    if (ret != 1)
        return -EINVAL;

    mutex_lock(&cpufreq_mtk_mutex);
    ret = set_min_cpu_freq(LITTLE, new_freq);
    mutex_unlock(&cpufreq_mtk_mutex);

    if (ret < 0)
        count = ret;

    return count;
}

cpufreq_mtk_attr(lcluster_min_freq);

static ssize_t show_lcluster_max_freq(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", current_cpu_freq[LITTLE].max);
}

static ssize_t store_lcluster_max_freq(struct kobject *kobj,
                    struct kobj_attribute *attr, const char *buf,
                    size_t count)
{
    int ret, new_freq;

    ret = sscanf(buf, "%d", &new_freq);
    if (ret != 1)
        return -EINVAL;

    mutex_lock(&cpufreq_mtk_mutex);
    ret = set_max_cpu_freq(LITTLE, new_freq);
    mutex_unlock(&cpufreq_mtk_mutex);

    if (ret < 0)
        count = ret;

    return count;
}

cpufreq_mtk_attr(lcluster_max_freq);

static ssize_t show_bcluster_min_freq(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
    return sprintf(buf, "%d\n", current_cpu_freq[BIG].min);
}

static ssize_t store_bcluster_min_freq(struct kobject *kobj,
                    struct kobj_attribute *attr, const char *buf,
                    size_t count)
{
    int ret, new_freq;

    ret = sscanf(buf, "%d", &new_freq);
    if (ret != 1)
        return -EINVAL;

    mutex_lock(&cpufreq_mtk_mutex);
    ret = set_min_cpu_freq(BIG, new_freq);
    mutex_unlock(&cpufreq_mtk_mutex);

    if (ret < 0)
        return ret;

    return count;
}

cpufreq_mtk_attr(bcluster_min_freq);

static ssize_t show_bcluster_max_freq(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
    return sprintf(buf, "%d\n", current_cpu_freq[BIG].max);
}

static ssize_t store_bcluster_max_freq(struct kobject *kobj,
                    struct kobj_attribute *attr, const char *buf,
                    size_t count)
{
    int ret, new_freq;

    ret = sscanf(buf, "%d", &new_freq);
    if (ret != 1)
        return -EINVAL;

    mutex_lock(&cpufreq_mtk_mutex);
    ret = set_max_cpu_freq(BIG, new_freq);
    mutex_unlock(&cpufreq_mtk_mutex);

    if (ret < 0)
        return ret;

    return count;
}

cpufreq_mtk_attr(bcluster_max_freq);

static struct attribute *mtk_param_attributes[] = {
    &lcluster_min_freq_attr.attr,
    &lcluster_max_freq_attr.attr,
    &bcluster_min_freq_attr.attr,
    &bcluster_max_freq_attr.attr,
    NULL,
};

static struct attribute_group mtk_param_attr_group = {
    .attrs = mtk_param_attributes,
    .name = "mtk",
};

static int __init cpufreq_mtk_init(void)
{
    int ret;

    current_cpu_freq = kcalloc(CLUSTER_NUM, sizeof(struct ppm_limit_data), GFP_KERNEL);

    if (!current_cpu_freq) {
        pr_err("[%s] Could not allocate memory for current_cpu_freq!\n", __func__);
        ret = -ENOMEM;
        goto out;
    }

    current_cpu_freq[LITTLE].min = -1;
    current_cpu_freq[BIG].min = -1;
    current_cpu_freq[LITTLE].max = -1;
    current_cpu_freq[BIG].max = -1;

    if (!cpufreq_global_kobject) {
        pr_err("[%s] !cpufreq_global_kobject\n", __func__);
        ret = -ENODEV;
        goto out;
    }

    ret = sysfs_create_group(cpufreq_global_kobject, &mtk_param_attr_group);
    if (ret) {
        pr_err("[%s] sysfs_create_group failed!\n", __func__);
        ret = -ENOMEM;
        goto out;
    }

out:
    return ret;
}

static void __exit cpufreq_mtk_exit(void)
{
    pr_debug("[%s] Driver unloading.", __func__);
    sysfs_remove_group(cpufreq_global_kobject, &mtk_param_attr_group);
    kfree(current_cpu_freq);
}

MODULE_DESCRIPTION("CPU frequencies setting for MTK scheduler");
MODULE_AUTHOR("bengris32");
MODULE_LICENSE("GPL");

module_init(cpufreq_mtk_init);
module_exit(cpufreq_mtk_exit);
