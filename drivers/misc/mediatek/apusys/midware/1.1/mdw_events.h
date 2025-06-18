/*
 * Copyright (C) 2020 MediaTek Inc.
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
#define TRACE_SYSTEM mdw_events
#if !defined(__MDW_EVENTS_H__) || defined(TRACE_HEADER_MULTI_READ)
#define __MDW_EVENTS_H__
#include <linux/tracepoint.h>
#include "mdw_rsc.h"
#define MDW_TAG_CMD_PRINT "%s,pid=%d,tgid=%d,cmd_uid=0x%llx,cmd_id=0x%llx,"\
	"sc_idx=%d,total_sc=%u,dev_type=%d,dev_name=%s,dev_idx=%d,"\
	"pack_id=0x%x,mc_idx=%u,mc_num=%u,mc_bitmap=0x%llx,priority=%d,"\
	"soft_limit=%u,hard_limit=%u,exec_time=%u,suggest_time=%u,"\
	"power_save=%d,mem_ctx=%u,tcm_force=%d,tcm_usage=0x%x,"\
	"tcm_real_usage=0x%x,boost=%u,ip_time=%u,ret=%d\n"\

#undef MDW_TAG_CMD_PRINT

#endif /* #if !defined(_MDW_EVENTS_H__) || defined(TRACE_HEADER_MULTI_READ) */


/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mdw_events
#include <trace/define_trace.h>

