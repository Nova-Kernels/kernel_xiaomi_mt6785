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
#include <linux/cpufreq.h>
#include <cpu_ctrl.h>
#include <sched_ctl.h>

#define CLUSTER_NUM 2
#define LITTLE 0
#define BIG 1

#define cpufreq_mtk_attr(_name)						\
static struct kobj_attribute _name##_attr =			\
__ATTR(_name, 0644, show_##_name, store_##_name)

static struct ppm_limit_data *current_cpu_freq;

extern int set_sched_boost(unsigned int val);

DEFINE_MUTEX(cpufreq_mtk_mutex);

/* Updates CPU frequency for chosen cluster */
void update_cpu_freq(int cluster)
{

#ifdef CONFIG_MTK_SCHED_BOOST
    int sched_boost_type = (current_cpu_freq[cluster].min > 0 || current_cpu_freq[cluster].max > 0)
                            ? SCHED_ALL_BOOST : SCHED_NO_BOOST;

    set_sched_boost(sched_boost_type);
#endif

    update_userlimit_cpu_freq(CPU_KIR_PERF, CLUSTER_NUM, current_cpu_freq);
}

/* Sets current maximum CPU frequency */
int set_max_cpu_freq(int cluster, int max)
{
    if (max < current_cpu_freq[cluster].min && current_cpu_freq[cluster].min > 0) {
        pr_err("[%s] Max freq cannot be lower than min freq!\n", __func__);
        return -EINVAL;
    }
    current_cpu_freq[cluster].max = max > 0 ? max : -1;
    update_cpu_freq(cluster);
    return 0;
}

/* Sets current minimum CPU frequency */
int set_min_cpu_freq(int cluster, int min)
{
    if (min > current_cpu_freq[cluster].max && current_cpu_freq[cluster].max > 0) {
        pr_err("[%s] Min freq cannot be higher than max freq!\n", __func__);
        return -EINVAL;
    }
    current_cpu_freq[cluster].min = min > 0 ? min : -1;
    update_cpu_freq(cluster);
    return 0;
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
        return ret;

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
        return ret;

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
