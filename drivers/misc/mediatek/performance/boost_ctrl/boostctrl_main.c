/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/proc_fs.h>

#include "boost_ctrl.h"

int init_boostctrl(struct proc_dir_entry *parent)
{
	struct proc_dir_entry *bstctrl_root = NULL;
#ifdef CONFIG_MTK_EAS_CTRL
	struct proc_dir_entry *easctrl_root = NULL;
#endif

	pr_debug("__init %s\n", __func__);


	bstctrl_root = proc_mkdir("boost_ctrl", parent);

#ifdef CONFIG_MTK_TOPO_CTRL
    /* init topology info first */
	topo_ctrl_init(bstctrl_root);
#endif

#ifdef CONFIG_MTK_CPU_CTRL
	cpu_ctrl_init(bstctrl_root);
#endif

	dram_ctrl_init(bstctrl_root);

#ifdef CONFIG_MTK_EAS_CTRL
	eas_ctrl_init(bstctrl_root);
	/* EAS */
	easctrl_root = proc_mkdir("eas_ctrl", bstctrl_root);
	uclamp_ctrl_init(easctrl_root);
	eas_ctrl_init(easctrl_root);
#endif

	return 0;
}
