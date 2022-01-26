/*
 * Copyright © 2015, Varun Chitre "varun.chitre15" <varun.chitre15@gmail.com>
 *
 * Driver modified by 7Soldier, 2022
 *
 * Charger Control driver for begonia charger.
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
 */
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/kernel.h>
#include <linux/thundercharge_control.h>

#define ENABLED             0
#define AC_CURRENT          3500
#define USB_CURRENT         900
#define MAX_VBUS_CURRENT    3500
#define MULTIPLIER          1000
#define THUNDERCHARGE       "thundercharge"

int mswitch = ENABLED;
int custom_ac_current = AC_CURRENT * MULTIPLIER;
int custom_usb_current = USB_CURRENT * MULTIPLIER;

#define DRIVER_VERSION  2
#define DRIVER_SUBVER 2

static ssize_t mswitch_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", mswitch);
}

static ssize_t mswitch_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int val;
	sscanf(buf, "%d", &val);
	switch(val)
	{
	case 0:
	case 1:
		mswitch = val;
	break;
	default:
		pr_info("%s: invalid value specified", THUNDERCHARGE);
    break;
}

return count;
}

static ssize_t cust_ac_current_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", custom_ac_current / MULTIPLIER);
}

static ssize_t cust_usb_current_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int newcurr;
	sscanf(buf, "%d", &newcurr);
	if(mswitch == 1 && newcurr <= MAX_VBUS_CURRENT)
		custom_usb_current = newcurr * MULTIPLIER;
	else
		pr_info("%s: disabled or limit reached, ignoring\n", THUNDERCHARGE);
	return count;
}

static ssize_t cust_usb_current_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", custom_usb_current / MULTIPLIER);
}

static ssize_t cust_ac_current_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int newcurr;
	sscanf(buf, "%d", &newcurr);
	if(mswitch == 1 && newcurr <= MAX_VBUS_CURRENT)
		custom_ac_current = newcurr * MULTIPLIER;
	else
		pr_info("%s: disabled or limit reached, ignoring\n", THUNDERCHARGE);
	return count;
}

static ssize_t chgr_ver_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "Charger Control %u.%u", DRIVER_VERSION, DRIVER_SUBVER);
}

static struct kobj_attribute mswitch_attribute =
	__ATTR(enabled,
		0664,
		mswitch_show,
		mswitch_store);

static struct kobj_attribute chgr_ctrl_ver_attribute =
	__ATTR(version,
		0444,
		chgr_ver_show, NULL);

static struct kobj_attribute cust_ac_current_attribute =
	__ATTR(custom_ac_current,
		0664,
		cust_ac_current_show,
		cust_ac_current_store);

static struct kobj_attribute cust_usb_current_attribute =
	__ATTR(custom_usb_current,
		0664,
		cust_usb_current_show,
		cust_usb_current_store);

static struct attribute *charger_control_attrs[] =
	{
		&cust_ac_current_attribute.attr,
		&cust_usb_current_attribute.attr,
		&mswitch_attribute.attr,
		&chgr_ctrl_ver_attribute.attr,
		NULL,
	};

static struct attribute_group chgr_control_attr_group =
	{
		.attrs = charger_control_attrs,
	};

static struct kobject *charger_control_kobj;

static int charger_control_probe(void)
{
	int sysfs_result;
	printk(KERN_DEBUG "[%s]\n",__func__);

	charger_control_kobj = kobject_create_and_add("thundercharge_control", kernel_kobj);

	if (!charger_control_kobj) {
		pr_err("%s Interface create failed!\n",
			__FUNCTION__);
		return -ENOMEM;
        }

	sysfs_result = sysfs_create_group(charger_control_kobj,
			&chgr_control_attr_group);

	if (sysfs_result) {
		pr_info("%s sysfs create failed!\n", __FUNCTION__);
		kobject_put(charger_control_kobj);
	}
	return sysfs_result;
}

static void charger_control_remove(void)
{
	if (charger_control_kobj != NULL)
		kobject_put(charger_control_kobj);
}

module_init(charger_control_probe);
module_exit(charger_control_remove);
MODULE_LICENSE("GPL and additional rights");
MODULE_AUTHOR("Varun Chitre <varun.chitre15@gmail.com>");
MODULE_DESCRIPTION("MTK Linear Charger control driver");
