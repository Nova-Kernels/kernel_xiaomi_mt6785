/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <mt-plat/mtk_gpu_utility.h>
#include <mtk_gpufreq.h>
#include "ged_ski.h"

static struct kobject *gpu_kobj;

ssize_t gpu_available_governor_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "Default\n");
}

static KOBJ_ATTR_RO(gpu_available_governor);

ssize_t gpu_busy_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int gpu_loading = 0;

	mtk_get_gpu_loading(&gpu_loading);

	return scnprintf(buf, PAGE_SIZE, "%u %%\n", gpu_loading);
}

static KOBJ_ATTR_RO(gpu_busy);

ssize_t gpu_clock_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int gpu_freq = 0;

	gpu_freq = mt_gpufreq_get_cur_volt() ? mt_gpufreq_get_cur_freq() : 0;

	return scnprintf(buf, PAGE_SIZE, "%u\n", gpu_freq / 1000);
}

static KOBJ_ATTR_RO(gpu_clock);

ssize_t gpu_freq_table_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct mt_gpufreq_power_table_info *power_table = NULL;
	unsigned int table_num = 0;
	unsigned int max_opp_idx = 0;
	char temp[1024] = {0};
	int idx;
	int count = 0;
	int pos = 0;
	int length;

	power_table = pass_gpu_table_to_eara();
	table_num = mt_gpufreq_get_dvfs_table_num();
	max_opp_idx = mt_gpufreq_get_seg_max_opp_index();

	for (idx = max_opp_idx; count < table_num; count++) {
		length = scnprintf(temp + pos, 1024 - pos,
				"%u ", power_table[idx + count].gpufreq_khz / 1000);
		pos += length;
	}

	return scnprintf(buf, PAGE_SIZE, "%s\n", temp);
}

static KOBJ_ATTR_RO(gpu_freq_table);

ssize_t gpu_governor_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "Default\n");
}

static KOBJ_ATTR_RO(gpu_governor);

static ssize_t gpu_max_clock_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int max_clock = 0;
	unsigned long max_clock_custom = 0;

	max_clock = mt_gpufreq_get_thermal_limit_freq();
	mtk_get_gpu_custom_upbound_freq(&max_clock_custom);
	max_clock = (max_clock_custom < max_clock) ?
			max_clock_custom : max_clock;

	return scnprintf(buf, PAGE_SIZE, "%u\n", max_clock / 1000);
}

static ssize_t gpu_max_clock_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int max_freq = 0;
	struct mt_gpufreq_power_table_info *power_table = NULL;
	unsigned int table_num = 0;
	unsigned int max_opp_idx = 0;
	int idx = 0;
	int index_count = 0;

	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &max_freq) == 0) {
				if (max_freq <= 0)
					return -EINVAL;

				power_table = pass_gpu_table_to_eara();
				table_num = mt_gpufreq_get_dvfs_table_num();
				max_opp_idx = mt_gpufreq_get_seg_max_opp_index();

				for (idx = max_opp_idx; index_count < table_num; index_count++) {
					if (max_freq ==
					    power_table[idx + index_count].gpufreq_khz) {
						mtk_custom_upbound_gpu_freq(index_count);
						return count;
					}
				}

				GED_LOGE("SKI: set max clock failed (%d not support)!\n", max_freq);
			}
		}
	}

	return -EINVAL;
}

static KOBJ_ATTR_RW(gpu_max_clock);

ssize_t gpu_min_clock_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned long min_clock = 0;
	unsigned long min_clock_custom = 0;

	mtk_get_gpu_bottom_freq(&min_clock);
	mtk_get_gpu_custom_boost_freq(&min_clock_custom);
	min_clock = (min_clock_custom > min_clock) ?
			min_clock_custom : min_clock;

	return scnprintf(buf, PAGE_SIZE, "%lu\n", min_clock / 1000);
}

static ssize_t gpu_min_clock_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int min_freq = 0;
	struct mt_gpufreq_power_table_info *power_table = NULL;
	unsigned int table_num = 0;
	unsigned int max_opp_idx = 0;
	int idx = 0;
	int index_count = 0;

	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &min_freq) == 0) {
				if (min_freq <= 0)
					return -EINVAL;

				power_table = pass_gpu_table_to_eara();
				table_num = mt_gpufreq_get_dvfs_table_num();
				max_opp_idx = mt_gpufreq_get_seg_max_opp_index();

				for (idx = max_opp_idx; index_count < table_num; index_count++) {
					if (min_freq ==
					    power_table[idx + index_count].gpufreq_khz) {
						mtk_custom_boost_gpu_freq(index_count);
						return count;
					}
				}

				GED_LOGE("SKI: set min clock failed (%d not support)!\n", min_freq);
			}
		}
	}

	return -EINVAL;
}

static KOBJ_ATTR_RW(gpu_min_clock);

ssize_t gpu_model_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
#if defined(CONFIG_MACH_MT6768)
	return scnprintf(buf, PAGE_SIZE, "Mali-G52 MC2\n"); //MT6768
#elif defined(CONFIG_MACH_MT6853)
	return scnprintf(buf, PAGE_SIZE, "Mali-G57 MC3\n"); //MT6853
#elif defined(CONFIG_MACH_MT6785)
	return scnprintf(buf, PAGE_SIZE, "Mali-G76 MP4\n"); //MT6785
#elif
	GED_LOGE("SKI: gpu model not set!\n");
	return scnprintf(buf, PAGE_SIZE, "UNKNOWN\n");
#endif
}

static KOBJ_ATTR_RO(gpu_model);

ssize_t gpu_tmu_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int temperature;

	temperature = mt_gpufreq_get_immed_gpu_temp();

	return scnprintf(buf, PAGE_SIZE, "%d\n", temperature);
}

static KOBJ_ATTR_RO(gpu_tmu);

GED_ERROR ged_ski_init(void)
{
	int ret = GED_OK;

	gpu_kobj = kobject_create_and_add("gpu", kernel_kobj);
	if (!gpu_kobj) {
		ret = GED_ERROR_OOM;
		GED_LOGE("ged: failed to create gpu_kobj!\n");
		goto EXIT;
	}

	ret = ged_sysfs_create_file(gpu_kobj, &kobj_attr_gpu_available_governor);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("ged: failed to create gpu_available_governor!\n");
		goto EXIT;
	}
	ret = ged_sysfs_create_file(gpu_kobj, &kobj_attr_gpu_busy);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("ged: failed to create gpu_busy!\n");
		goto EXIT;
	}
	ret = ged_sysfs_create_file(gpu_kobj, &kobj_attr_gpu_clock);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("ged: failed to create gpu_clock!\n");
		goto EXIT;
	}
	ret = ged_sysfs_create_file(gpu_kobj, &kobj_attr_gpu_freq_table);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("ged: failed to create gpu_freq_table!\n");
		goto EXIT;
	}
	ret = ged_sysfs_create_file(gpu_kobj, &kobj_attr_gpu_governor);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("ged: failed to create gpu_governor!\n");
		goto EXIT;
	}
	ret = ged_sysfs_create_file(gpu_kobj, &kobj_attr_gpu_max_clock);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("ged: failed to create gpu_max_clock!\n");
		goto EXIT;
	}
	ret = ged_sysfs_create_file(gpu_kobj, &kobj_attr_gpu_min_clock);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("ged: failed to create gpu_min_clock!\n");
		goto EXIT;
	}
	ret = ged_sysfs_create_file(gpu_kobj, &kobj_attr_gpu_model);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("ged: failed to create gpu_model!\n");
		goto EXIT;
	}
	ret = ged_sysfs_create_file(gpu_kobj, &kobj_attr_gpu_tmu);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("ged: failed to create gpu_tmu!\n");
		goto EXIT;
	}

	return ret;

EXIT:
	ged_ski_exit();
	return ret;
}

void ged_ski_exit(void)
{
	ged_sysfs_remove_file(gpu_kobj, &kobj_attr_gpu_available_governor);
	ged_sysfs_remove_file(gpu_kobj, &kobj_attr_gpu_busy);
	ged_sysfs_remove_file(gpu_kobj, &kobj_attr_gpu_clock);
	ged_sysfs_remove_file(gpu_kobj, &kobj_attr_gpu_freq_table);
	ged_sysfs_remove_file(gpu_kobj, &kobj_attr_gpu_governor);
	ged_sysfs_remove_file(gpu_kobj, &kobj_attr_gpu_max_clock);
	ged_sysfs_remove_file(gpu_kobj, &kobj_attr_gpu_min_clock);
	ged_sysfs_remove_file(gpu_kobj, &kobj_attr_gpu_model);
	ged_sysfs_remove_file(gpu_kobj, &kobj_attr_gpu_tmu);
	kobject_put(gpu_kobj);
	gpu_kobj = NULL;
}
