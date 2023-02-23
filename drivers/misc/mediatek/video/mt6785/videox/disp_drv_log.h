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

#ifndef __DISP_DRV_LOG_H__
#define __DISP_DRV_LOG_H__

#include "display_recorder.h"
#include "ddp_debug.h"
#ifdef CONFIG_MTK_AEE_FEATURE
#include "mt-plat/aee.h"
#endif

#define DISP_LOG_PRINT(level, sub_module, fmt, args...)			\
			dprec_logger_pr(DPREC_LOGGER_DEBUG, fmt, ##args)

#define DISPINFO(string, args...)					\
	do {								\
		dprec_logger_pr(DPREC_LOGGER_DEBUG, string, ##args);	\
		if (g_mobilelog)					\
			pr_debug("[DISP]"string, ##args);		\
	} while (0)

#define DISPMSG(string, args...)					\
	do {								\
		dprec_logger_pr(DPREC_LOGGER_DEBUG, string, ##args);	\
		pr_debug("[DISP]"string, ##args);			\
	} while (0)

#define DISPCHECK(string, args...)					\
	do {								\
		dprec_logger_pr(DPREC_LOGGER_DEBUG, string, ##args);	\
		pr_debug("[DISP]"string, ##args);			\
	} while (0)

#define DISP_ONESHOT_DUMP(string, args...)				\
	do {								\
		dprec_logger_pr(DPREC_LOGGER_ONESHOT_DUMP, string, ##args); \
		pr_debug("[DISP]"string, ##args);			\
	} while (0)

#define DISP_PR_INFO(string, args...)					\
	do {								\
		dprec_logger_pr(DPREC_LOGGER_ERROR, string, ##args);	\
		pr_warn("[DISP][%s #%d]warn:"string,			\
				__func__, __LINE__, ##args);		\
	} while (0)

#define DISP_PR_ERR(string, args...)					\
	do {								\
		dprec_logger_pr(DPREC_LOGGER_ERROR, string, ##args);	\
		pr_err("[DISP][%s #%d]ERROR:"string,			\
				__func__, __LINE__, ##args);		\
	} while (0)

#define DISPFENCE(string, args...)					\
	do {								\
		dprec_logger_pr(DPREC_LOGGER_FENCE, string, ##args);	\
		if (g_mobilelog)					\
			pr_debug("fence/"string, ##args);		\
	} while (0)

#define DISPDBG(string, args...)					\
	do {								\
		if (ddp_debug_dbg_log_level())				\
			DISPMSG(string, ##args);			\
	} while (0)

#define DISPFUNC()							\
	do {								\
		dprec_logger_pr(DPREC_LOGGER_DEBUG, "func|%s\n", __func__); \
		if (g_mobilelog)					\
			pr_debug("%s line:%d", __func__, __LINE__);\
	} while (0)

#define DISPDBGFUNC() DISPFUNC()

#define DISPPR_HWOP(string, args...)

#define _DISP_PRINT_FENCE_OR_ERR(is_err, string, args...)		\
	do {								\
		if (is_err)						\
			DISP_PR_ERR(string, ##args);			\
		else							\
			DISPFENCE(string, ##args);			\
	} while (0)

#endif /* __DISP_DRV_LOG_H__ */
