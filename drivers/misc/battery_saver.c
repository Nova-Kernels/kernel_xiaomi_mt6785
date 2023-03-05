// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Adithya R <gh0strider.2k18.reborn@gmail.com>.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/battery_saver.h>

static bool enabled = false;

// returns whether battery saver is enabled or disabled
bool is_battery_saver_on(void)
{
	return enabled;
}

// enable or disable battery saver mode
void update_battery_saver(bool status)
{
	enabled = status;
}

static int set_enabled(const char *val, const struct kernel_param *kp)
{
	long state;
	int res = 0;

	if(!strcmp(val, "Y"))
		state = 1;
	else if(!strcmp(val, "N"))
		state = 0;
	else
		res = kstrtol(val, 0, &state);

	if (res != 0 || state < 0 || state > 1)
		return -EINVAL;

	if(state)
		printk(KERN_INFO "Entering battery saving mode\n");
	else
		printk(KERN_INFO "Leaving battery saving mode\n");

	enabled = state;
	return 0;
}

static const struct kernel_param_ops battery_saver_ops =
{
	.set = &set_enabled,
	.get = &param_get_bool,
};

module_param_cb(enabled, &battery_saver_ops, &enabled, S_IRUGO|S_IWUSR );