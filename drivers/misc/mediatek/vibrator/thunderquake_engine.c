/*
 * Copyright � 2014, Varun Chitre "varun.chitre15" <varun.chitre15@gmail.com>
 *
 * Vibration Intensity Controller for MTK Vibrator
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Please preserve this licence and driver name if you implement this
 * anywhere else.
 *
 */

#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>

#include <vibrator.h>
#include <vibrator_hal.h>
#include <mt-plat/upmu_common.h>

#define MAX_VIBR 9
#define MIN_VIBR 0

#define ENGINE_VERSION  1
#define ENGINE_VERSION_SUB 0

extern unsigned short pmic_set_register_value(PMU_FLAGS_LIST_ENUM flagname, uint32_t val);

static ssize_t vibr_vtg_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct vibrator_hw* hw = mt_get_cust_vibrator_hw();

	return sprintf(buf, "%d\n", hw->vib_vol);
}

static ssize_t vibr_vtg_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int val;
	struct vibrator_hw* hw = mt_get_cust_vibrator_hw();
	sscanf(buf, "%u", &val);
	if(val>=MIN_VIBR && val <=MAX_VIBR) {
		pmic_set_register_value(PMIC_RG_VIBR_VOSEL, val);
		hw->vib_vol=val;
	}

	return count;
}

static ssize_t thunderquake_version_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "version: %u.%u\n", ENGINE_VERSION, ENGINE_VERSION_SUB);
}

static ssize_t min_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", MIN_VIBR);
}

static ssize_t max_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", MAX_VIBR);
}

static struct kobj_attribute thunderquake_version_attribute =
	__ATTR(engine_version,
		0444,
		thunderquake_version_show, NULL);

static struct kobj_attribute thunderquake_level_attribute =
	__ATTR(level,
		0664,
		vibr_vtg_show, vibr_vtg_store);

static struct kobj_attribute thunderquake_min_attribute =
	__ATTR(min,
		0444,
		min_show, NULL);


static struct kobj_attribute thunderquake_max_attribute =
	__ATTR(max,
		0444,
		max_show, NULL);


static struct attribute *thunderquake_engine_attrs[] =
	{
		&thunderquake_level_attribute.attr,
		&thunderquake_version_attribute.attr,
		&thunderquake_max_attribute.attr,
		&thunderquake_min_attribute.attr,
		NULL,
	};

static struct attribute_group vibr_level_control_attr_group =
	{
		.attrs = thunderquake_engine_attrs,
	};

static struct kobject *vibr_level_control_kobj;

static int vibr_level_control_init(void)
{
	int sysfs_result;
	printk(KERN_DEBUG "[%s]\n",__func__);

	vibr_level_control_kobj =
		kobject_create_and_add("thunderquake_engine", kernel_kobj);

	if (!vibr_level_control_kobj) {
		pr_err("%s Interface create failed!\n",
			__FUNCTION__);
		return -ENOMEM;
	}

	sysfs_result = sysfs_create_group(vibr_level_control_kobj,
			&vibr_level_control_attr_group);

	if (sysfs_result) {
		pr_info("%s sysfs create failed!\n", __FUNCTION__);
		kobject_put(vibr_level_control_kobj);
	}
	return sysfs_result;
}

static void vibr_level_control_exit(void)
{
	if (vibr_level_control_kobj != NULL)
		kobject_put(vibr_level_control_kobj);
}

module_init(vibr_level_control_init);
module_exit(vibr_level_control_exit);
MODULE_LICENSE("GPL and additional rights");
MODULE_AUTHOR("Varun Chitre <varun.chitre15@gmail.com>");
MODULE_DESCRIPTION("ThundQuake Engine - Driver to control Mediatek Vibrator Intensity");
