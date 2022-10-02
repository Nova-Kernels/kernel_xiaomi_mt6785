/*
 * Copyright (c) 2015 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <drm/drmP.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_notifier_mi.h>
#include <linux/clk.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/component.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <video/mipi_display.h>
#include <video/videomode.h>
#include <linux/soc/mediatek/mtk-cmdq.h>
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) \
	|| defined(CONFIG_MACH_MT6833)
#include <linux/ratelimit.h>
#endif

#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_helper.h"
#include "mtk_mipi_tx.h"
#include "mtk_dump.h"
#include "mtk_log.h"
#include "mtk_drm_lowpower.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_arr.h"
#include "mtk_panel_ext.h"

#ifdef CONFIG_HWCONF_MANAGER
#include "dsi_panel_mi_count.h"
#endif

#define DSI_READ_WRITE_PANEL_DEBUG 1
#if DSI_READ_WRITE_PANEL_DEBUG
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif

/* ************ Panel Master ********** */
#include "mtk_drm_fbdev.h"
#include "mtk_fbconfig_kdebug.h"
/* ********* end Panel Master *********** */
#define DSI_START 0x00
#define SLEEPOUT_START BIT(2)
#define VM_CMD_START BIT(16)
#define START_FLD_REG_START REG_FLD_MSB_LSB(0, 0)

#define DSI_INTEN 0x08

#define DSI_INTSTA 0x0c
#define LPRX_RD_RDY_INT_FLAG BIT(0)
#define CMD_DONE_INT_FLAG BIT(1)
#define TE_RDY_INT_FLAG BIT(2)
#define VM_DONE_INT_FLAG BIT(3)
#define FRAME_DONE_INT_FLAG BIT(4)
#define VM_CMD_DONE_INT_EN BIT(5)
#define SLEEPOUT_DONE_INT_FLAG BIT(6)
#define BUFFER_UNDERRUN_INT_FLAG BIT(12)
#define INP_UNFINISH_INT_EN BIT(14)
#define SLEEPIN_ULPS_DONE_INT_FLAG BIT(15)
#define DSI_BUSY BIT(31)
#define INTSTA_FLD_REG_RD_RDY REG_FLD_MSB_LSB(0, 0)
#define INTSTA_FLD_REG_CMD_DONE REG_FLD_MSB_LSB(1, 1)
#define INTSTA_FLD_REG_TE_RDY REG_FLD_MSB_LSB(2, 2)
#define INTSTA_FLD_REG_VM_DONE REG_FLD_MSB_LSB(3, 3)
#define INTSTA_FLD_REG_FRM_DONE REG_FLD_MSB_LSB(4, 4)
#define INTSTA_FLD_REG_VM_CMD_DONE REG_FLD_MSB_LSB(5, 5)
#define INTSTA_FLD_REG_SLEEPOUT_DONE REG_FLD_MSB_LSB(6, 6)
#define INTSTA_FLD_REG_BUSY REG_FLD_MSB_LSB(31, 31)

#define DSI_CON_CTRL 0x10
#define DSI_RESET BIT(0)
#define DSI_EN BIT(1)
#define DSI_PHY_RESET BIT(2)
#define CON_CTRL_FLD_REG_DUAL_EN REG_FLD_MSB_LSB(4, 4)
#define DSI_CM_WAIT_FIFO_FULL_EN BIT(27)

#define DSI_MODE_CTRL 0x14
#define MODE (3)
#define CMD_MODE 0
#define SYNC_PULSE_MODE 1
#define SYNC_EVENT_MODE 2
#define BURST_MODE 3
#define FRM_MODE BIT(16)
#define MIX_MODE BIT(17)
#define SLEEP_MODE BIT(20)
#define MODE_FLD_REG_MODE_CON REG_FLD_MSB_LSB(1, 0)

#define DSI_TXRX_CTRL 0x18
#define VC_NUM BIT(1)
#define LANE_NUM (0xf << 2)
#define DIS_EOT BIT(6)
#define NULL_EN BIT(7)
#define TE_FREERUN BIT(8)
#define EXT_TE_EN BIT(9)
#define EXT_TE_EDGE BIT(10)
#define MAX_RTN_SIZE (0xf << 12)
#define HSTX_CKLP_EN BIT(16)
#define TXRX_CTRL_FLD_REG_LANE_NUM REG_FLD_MSB_LSB(5, 2)
#define TXRX_CTRL_FLD_REG_EXT_TE_EN REG_FLD_MSB_LSB(9, 9)
#define TXRX_CTRL_FLD_REG_EXT_TE_EDGE REG_FLD_MSB_LSB(10, 10)
#define TXRX_CTRL_FLD_REG_HSTX_CKLP_EN REG_FLD_MSB_LSB(16, 16)

#define DSI_PSCTRL 0x1c
#define DSI_PS_WC	REG_FLD_MSB_LSB(14, 0)
#define DSI_PS_SEL	REG_FLD_MSB_LSB(18, 16)

#define DSI_VSA_NL 0x20
#define DSI_VBP_NL 0x24
#define DSI_VFP_NL 0x28
#define DSI_SIZE_CON 0x38
#define DSI_VACT_NL 0x2C
#define DSI_LFR_CON 0x30
#define DSI_LFR_STA 0x34
#define LFR_STA_FLD_REG_LFR_SKIP_STA REG_FLD_MSB_LSB(8, 8)
#define LFR_STA_FLD_REG_LFR_SKIP_CNT REG_FLD_MSB_LSB(5, 0)
#define LFR_CON_FLD_REG_LFR_MODE REG_FLD_MSB_LSB(1, 0)
#define LFR_CON_FLD_REG_LFR_TYPE REG_FLD_MSB_LSB(3, 2)
#define LFR_CON_FLD_REG_LFR_EN REG_FLD_MSB_LSB(4, 4)
#define LFR_CON_FLD_REG_LFR_UPDATE REG_FLD_MSB_LSB(5, 5)
#define LFR_CON_FLD_REG_LFR_VSE_DIS REG_FLD_MSB_LSB(6, 6)
#define LFR_CON_FLD_REG_LFR_SKIP_NUM REG_FLD_MSB_LSB(13, 8)

#define DSI_HSA_WC 0x50
#define DSI_HBP_WC 0x54
#define DSI_HFP_WC 0x58
#define DSI_BLLP_WC 0x5C

#define DSI_CMDQ_SIZE 0x60
#define CMDQ_SIZE 0x3f

#define DSI_HSTX_CKL_WC 0x64

#define DSI_RX_DATA0 0x74
#define DSI_RX_DATA1 0x78
#define DSI_RX_DATA2 0x7c
#define DSI_RX_DATA3 0x80

#define DSI_RACK 0x84
#define RACK BIT(0)

#define DSI_MEM_CONTI 0x90
#define DSI_WMEM_CONTI 0x3C

#define DSI_TIME_CON0 0xA0

#define DSI_PHY_LCCON 0x104
#define LC_HS_TX_EN BIT(0)
#define LC_ULPM_EN BIT(1)
#define LC_WAKEUP_EN BIT(2)
#define PHY_FLD_REG_LC_HSTX_EN REG_FLD_MSB_LSB(0, 0)

#define DSI_PHY_LD0CON 0x108
#define LD0_HS_TX_EN BIT(0)
#define LD0_ULPM_EN BIT(1)
#define LD0_WAKEUP_EN BIT(2)
#define LDX_ULPM_AS_L0 BIT(3)

#define DSI_PHY_TIMECON0 0x110
#define LPX (0xff << 0)
#define HS_PREP (0xff << 8)
#define HS_ZERO (0xff << 16)
#define HS_TRAIL (0xff << 24)
#define FLD_LPX REG_FLD_MSB_LSB(7, 0)
#define FLD_HS_PREP REG_FLD_MSB_LSB(15, 8)
#define FLD_HS_ZERO REG_FLD_MSB_LSB(23, 16)
#define FLD_HS_TRAIL REG_FLD_MSB_LSB(31, 24)

#define DSI_PHY_TIMECON1 0x114
#define TA_GO (0xff << 0)
#define TA_SURE (0xff << 8)
#define TA_GET (0xff << 16)
#define DA_HS_EXIT (0xff << 24)
#define FLD_TA_GO REG_FLD_MSB_LSB(7, 0)
#define FLD_TA_SURE REG_FLD_MSB_LSB(15, 8)
#define FLD_TA_GET REG_FLD_MSB_LSB(23, 16)
#define FLD_DA_HS_EXIT REG_FLD_MSB_LSB(31, 24)

#define DSI_PHY_TIMECON2 0x118
#define CONT_DET (0xff << 0)
#define CLK_ZERO (0xff << 16)
#define CLK_TRAIL (0xff << 24)
#define FLD_CONT_DET REG_FLD_MSB_LSB(7, 0)
#define FLD_DA_HS_SYNC REG_FLD_MSB_LSB(15, 8)
#define FLD_CLK_HS_ZERO REG_FLD_MSB_LSB(23, 16)
#define	FLD_CLK_HS_TRAIL REG_FLD_MSB_LSB(31, 24)

#define DSI_PHY_TIMECON3 0x11c
#define CLK_HS_PREP (0xff << 0)
#define CLK_HS_POST (0xff << 8)
#define CLK_HS_EXIT (0xff << 16)
#define FLD_CLK_HS_PREP REG_FLD_MSB_LSB(7, 0)
#define FLD_CLK_HS_POST REG_FLD_MSB_LSB(15, 8)
#define FLD_CLK_HS_EXIT REG_FLD_MSB_LSB(23, 16)
#define DSI_CPHY_CON0 0x120

#define DSI_VM_CMD_CON 0x130
#define VM_CMD_EN BIT(0)
#define TS_VFP_EN BIT(5)
#define DSI_VM_CMD_DATA0	0x134
#define DSI_VM_CMD_DATA10	0x180
#define DSI_VM_CMD_DATA20	0x1A0
#define DSI_VM_CMD_DATA30	0x1B0

#define DSI_VM_CMD_DATA0 0x134

#define DSI_STATE_DBG6 0x160
#define STATE_DBG6_FLD_REG_CMCTL_STATE REG_FLD_MSB_LSB(14, 0)

#define DSI_SHADOW_DEBUG 0x190
#define DSI_BYPASS_SHADOW BIT(1)
#define DSI_READ_WORKING BIT(2)

#define DSI_CMDQ0 0x200
#define DSI_CMDQ1 0x204

#define CONFIG (0xff << 0)
#define SHORT_PACKET 0
#define LONG_PACKET 2
#define VM_LONG_PACKET BIT(1)
#define BTA BIT(2)
#define HSTX BIT(3)
#define DATA_ID (0xff << 8)
#define DATA_0 (0xff << 16)
#define DATA_1 (0xff << 24)

#define MMSYS_SW_RST_DSI_B BIT(2)
#define MMSYS_SW_RST_DSI1_B BIT(3)

#define DSI_START_FLD_DSI_START REG_FLD_MSB_LSB(0, 0)
#define DSI_INSTA_FLD_DSI_BUSY REG_FLD_MSB_LSB(31, 31)
#define DSI_COM_CON_FLD_DUAL_EN REG_FLD_MSB_LSB(4, 4)
#define DSI_MODE_CON_FLD_MODE_CON REG_FLD_MSB_LSB(1, 0)

#define T_LPX (8)
#define T_HS_PREP (7)
#define T_HS_TRAIL (8)
#define T_HS_EXIT (16)
#define T_HS_ZERO (15)
#define DA_HS_SYNC (1)

#define NS_TO_CYCLE(n, c) ((n) / (c))

#define MTK_DSI_HOST_IS_READ(type)                                             \
	((type == MIPI_DSI_GENERIC_READ_REQUEST_0_PARAM) ||                    \
	 (type == MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM) ||                    \
	 (type == MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM) ||                    \
	 (type == MIPI_DSI_DCS_READ))

struct phy;
struct mtk_dsi;
struct drm_notifier_data g_notify_data;

static struct mtk_dsi *g_mtk_dsi;

#define DSI_DCS_SHORT_PACKET_ID_0 0x05
#define DSI_DCS_SHORT_PACKET_ID_1 0x15
#define DSI_DCS_LONG_PACKET_ID 0x39
#define DSI_DCS_READ_PACKET_ID 0x06

#define DSI_GERNERIC_SHORT_PACKET_ID_1 0x13
#define DSI_GERNERIC_SHORT_PACKET_ID_2 0x23
#define DSI_GERNERIC_LONG_PACKET_ID 0x29
#define DSI_GERNERIC_READ_LONG_PACKET_ID 0x14

struct DSI_T0_INS {
	unsigned CONFG : 8;
	unsigned Data_ID : 8;
	unsigned Data0 : 8;
	unsigned Data1 : 8;
};

#define DECLARE_DSI_PORCH(EXPR)                                                \
	EXPR(DSI_VFP)                                                          \
	EXPR(DSI_VSA)                                                          \
	EXPR(DSI_VBP)                                                          \
	EXPR(DSI_VACT)                                                         \
	EXPR(DSI_HFP)                                                          \
	EXPR(DSI_HSA)                                                          \
	EXPR(DSI_HBP)                                                          \
	EXPR(DSI_BLLP)                                                         \
	EXPR(DSI_PORCH_NUM)

enum dsi_porch_type { DECLARE_DSI_PORCH(DECLARE_NUM) };

static const char * const mtk_dsi_porch_str[] = {
	DECLARE_DSI_PORCH(DECLARE_STR)};

#define AS_UINT32(x) (*(u32 *)((void *)x))

struct mtk_dsi_driver_data {
	const u32 reg_cmdq_ofs;
	s32 (*poll_for_idle)(struct mtk_dsi *dsi, struct cmdq_pkt *handle);
	irqreturn_t (*irq_handler)(int irq, void *dev_id);
	char *esd_eint_compat;
	bool support_shadow;
};

struct t_condition_wq {
	wait_queue_head_t wq;
	atomic_t condition;
};

struct LCM_setting_table {
	unsigned cmd;
	unsigned char count;
	unsigned char para_list[64];
};

struct LCM_mipi_read_write {
	unsigned int read_enable;
	unsigned int read_count;
	unsigned char read_buffer[64];
	struct LCM_setting_table lcm_setting_table;
};

struct mtk_dsi {
	struct mtk_ddp_comp ddp_comp;
	struct device *dev;
	struct mipi_dsi_host host;
	struct drm_encoder encoder;
	struct drm_connector conn;
	struct drm_panel *panel;
	struct mtk_panel_ext *ext;
	struct cmdq_pkt_buffer cmdq_buf;
	struct drm_bridge *bridge;
	struct phy *phy;
	struct mutex dsi_lock;
	void __iomem *regs;

	struct clk *engine_clk;
	struct clk *digital_clk;
	struct clk *hs_clk;

	u32 data_rate;

	unsigned long mode_flags;
	enum mipi_dsi_pixel_format format;
	unsigned int lanes;
	struct videomode vm;
	int clk_refcnt;
	int  doze_state;
	bool output_en;
	bool doze_enabled;
	u32 irq_data;
	wait_queue_head_t irq_wait_queue;
	struct mtk_dsi_driver_data *driver_data;

	struct t_condition_wq enter_ulps_done;
	struct t_condition_wq exit_ulps_done;
	struct t_condition_wq te_rdy;
	struct t_condition_wq frame_done;
	unsigned int hs_trail;
	unsigned int hs_prpr;
	unsigned int hs_zero;
	unsigned int lpx;
	unsigned int ta_get;
	unsigned int ta_sure;
	unsigned int ta_go;
	unsigned int da_hs_exit;
	unsigned int cont_det;
	unsigned int clk_zero;
	unsigned int clk_hs_prpr;
	unsigned int clk_hs_exit;
	unsigned int clk_hs_post;

	unsigned int vsa;
	unsigned int vbp;
	unsigned int vfp;
	unsigned int hsa_byte;
	unsigned int hbp_byte;
	unsigned int hfp_byte;

	bool mipi_hopping_sta;
	bool panel_osc_hopping_sta;
	bool fod_hbm_flag;
	bool normal_hbm_flag;
	bool thermal_hbm_disabled;
	bool dc_flag;
	bool doze_flag;
	bool esd_recovery_flag;
	unsigned int data_phy_cycle;
	bool fod_backlight_flag;
	/* for Panel Master dcs read/write */
	struct mipi_dsi_device *dev_for_PM;
};

enum DSI_MODE_CON {
	MODE_CON_CMD = 0,
	MODE_CON_SYNC_PULSE_VDO,
	MODE_CON_SYNC_EVENT_VDO,
	MODE_CON_BURST_VDO,
};

static struct LCM_mipi_read_write lcm_mipi_read_write ={0};

struct mtk_panel_ext *mtk_dsi_get_panel_ext(struct mtk_ddp_comp *comp);

static inline struct mtk_dsi *encoder_to_dsi(struct drm_encoder *e)
{
	return container_of(e, struct mtk_dsi, encoder);
}

static inline struct mtk_dsi *connector_to_dsi(struct drm_connector *c)
{
	return container_of(c, struct mtk_dsi, conn);
}

struct drm_connector *dsi_to_connector(void *dsi)
{
	return &(((struct mtk_dsi *)dsi)->conn);
}

static inline struct mtk_dsi *host_to_dsi(struct mipi_dsi_host *h)
{
	return container_of(h, struct mtk_dsi, host);
}

static void mtk_dsi_mask(struct mtk_dsi *dsi, u32 offset, u32 mask, u32 data)
{
	u32 temp = readl(dsi->regs + offset);

	writel((temp & ~mask) | (data & mask), dsi->regs + offset);
}

#define CHK_SWITCH(a, b)  ((a == 0) ? b : a)

int mtk_fod_backlight_flag(struct drm_connector *c)
{
	struct mtk_dsi *dsi = container_of(c, struct mtk_dsi, conn);
	if (!dsi) {
		DDPPR_ERR("%s dsi is null\n", __func__);
		return -1;
	}
	return dsi->fod_backlight_flag;
}

int mtk_dc_flag(struct drm_connector *c)
{
	struct mtk_dsi *dsi = container_of(c, struct mtk_dsi, conn);
	if (!dsi) {
		DDPPR_ERR("%s dsi is null\n", __func__);
		return -1;
	}
	return dsi->dc_flag;
}

int mtk_doze_flag(struct drm_connector *c)
{
	struct mtk_dsi *dsi = container_of(c, struct mtk_dsi, conn);
	if (!dsi) {
		DDPPR_ERR("%s dsi is null\n", __func__);
		return -1;
	}
	return dsi->doze_flag;
}

int mtk_clear_fod_status(struct drm_connector *c)
{
	struct mtk_dsi *dsi = container_of(c, struct mtk_dsi, conn);
	if (!dsi) {
		DDPPR_ERR("%s dsi is null\n", __func__);
		return -1;
	}

	pr_info("%s\n", __func__);
	dsi->fod_backlight_flag = false;
	dsi->fod_hbm_flag = false;
	dsi->normal_hbm_flag = false;
	return 0;
}

int mtk_fod_hbm_flag(struct drm_connector *c)
{
	struct mtk_dsi *dsi = container_of(c, struct mtk_dsi, conn);
	if (!dsi) {
		DDPPR_ERR("%s dsi is null\n", __func__);
		return -1;
	}
	return dsi->fod_hbm_flag;
}

int mtk_normal_hbm_flag(struct drm_connector *c)
{
	struct mtk_dsi *dsi = container_of(c, struct mtk_dsi, conn);
	if (!dsi) {
		DDPPR_ERR("%s dsi is null\n", __func__);
		return -1;
	}
	return dsi->normal_hbm_flag;
}

void mtk_notify_backlight_node(unsigned int backlight)
{
	if(!g_mtk_dsi) {
		DDPPR_ERR("%s dsi is null\n", __func__);
		return;
	}

	mutex_lock(&g_mtk_dsi->dsi_lock);
	if(g_mtk_dsi->dc_flag && backlight < g_mtk_dsi->ext->params->dc_backlight_threhold) {
		g_mtk_dsi->conn.brightness_clone = backlight;
		sysfs_notify(&g_mtk_dsi->conn.kdev->kobj, NULL, "brightness_clone");
	}
	mutex_unlock(&g_mtk_dsi->dsi_lock);
	return;
}

static bool mtk_dsi_doze_state(struct mtk_dsi *dsi)
{
	struct drm_crtc *crtc = dsi->encoder.crtc;
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc->state);

	return state->prop_val[CRTC_PROP_DOZE_ACTIVE];
}

static bool mtk_dsi_doze_status_change(struct mtk_dsi *dsi)
{
	bool doze_enabled = mtk_dsi_doze_state(dsi);

	if (dsi->doze_enabled == doze_enabled)
		return false;
	return true;
}

static void mtk_dsi_dphy_timconfig(struct mtk_dsi *dsi, void *handle)
{

	struct mtk_dsi_phy_timcon *phy_timcon = NULL;
	u32 lpx = 0, hs_prpr = 0, hs_zero = 0, hs_trail = 0;
	u32 ta_get = 0, ta_sure = 0, ta_go = 0, da_hs_exit = 0;
	u32 clk_zero = 0, clk_trail = 0, da_hs_sync = 0;
	u32 clk_hs_prpr = 0, clk_hs_exit = 0, clk_hs_post = 0;
	u32 cont_det = 0;
	u32 ui = 0, cycle_time = 0;
	u32 value = 0;
	struct mtk_ddp_comp *comp = &dsi->ddp_comp;

	ui = 1000 / dsi->data_rate + 0x01;
	cycle_time = 8000 / dsi->data_rate + 0x01;

	lpx = NS_TO_CYCLE(dsi->data_rate * 0x4B, 0x1F40) + 0x1;
	hs_prpr = NS_TO_CYCLE((0x40 + 0x5 * ui), cycle_time) + 0x1;
	hs_zero = NS_TO_CYCLE((0xC8 + 0x0A * ui), cycle_time);
	hs_zero = hs_zero > hs_prpr ? hs_zero - hs_prpr : hs_zero;
	hs_trail = NS_TO_CYCLE((0x4 * ui + 0x50) *
		dsi->data_rate, 0x1F40) + 0x1;

	ta_get = 5 * lpx;
	ta_sure = 3 * lpx / 2;
	ta_go = 4 * lpx;
	da_hs_exit = 2 * lpx;

	clk_zero = NS_TO_CYCLE(0x190, cycle_time);
	clk_trail = NS_TO_CYCLE(0x64 * dsi->data_rate, 0x1F40) + 0x1;
	da_hs_sync = 0x1;
	cont_det = 0x3;

	clk_hs_prpr = NS_TO_CYCLE(0x50 * dsi->data_rate, 0x1F40);
	clk_hs_exit = 2 * lpx;
	clk_hs_post = NS_TO_CYCLE(0x60 + 0x34 * ui, cycle_time);

	if (!(dsi->ext && dsi->ext->params))
		goto CONFIG_REG;

	phy_timcon = &dsi->ext->params->phy_timcon;

	lpx = CHK_SWITCH(phy_timcon->lpx, lpx);
	hs_prpr = CHK_SWITCH(phy_timcon->hs_prpr, hs_prpr);
	hs_zero = CHK_SWITCH(phy_timcon->hs_zero, hs_zero);
	hs_trail = CHK_SWITCH(phy_timcon->hs_trail, hs_trail);

	ta_get = CHK_SWITCH(phy_timcon->ta_get, ta_get);
	ta_sure = CHK_SWITCH(phy_timcon->ta_sure, ta_sure);
	ta_go = CHK_SWITCH(phy_timcon->ta_go, ta_go);
	da_hs_exit = CHK_SWITCH(phy_timcon->da_hs_exit, da_hs_exit);

	clk_zero = CHK_SWITCH(phy_timcon->clk_zero, clk_zero);
	clk_trail = CHK_SWITCH(phy_timcon->clk_trail, clk_trail);
	da_hs_sync = CHK_SWITCH(phy_timcon->da_hs_sync, da_hs_sync);

	clk_hs_prpr = CHK_SWITCH(phy_timcon->clk_hs_prpr, clk_hs_prpr);
	clk_hs_exit = CHK_SWITCH(phy_timcon->clk_hs_exit, clk_hs_exit);
	clk_hs_post = CHK_SWITCH(phy_timcon->clk_hs_post, clk_hs_post);

CONFIG_REG:
	value = REG_FLD_VAL(FLD_LPX, lpx)
		| REG_FLD_VAL(FLD_HS_PREP, hs_prpr)
		| REG_FLD_VAL(FLD_HS_ZERO, hs_zero)
		| REG_FLD_VAL(FLD_HS_TRAIL, hs_trail);

	if (handle)
		cmdq_pkt_write((struct cmdq_pkt *)handle, comp->cmdq_base,
			comp->regs_pa+DSI_PHY_TIMECON0, value, ~0);
	else
		writel(value, dsi->regs + DSI_PHY_TIMECON0);

	value = REG_FLD_VAL(FLD_TA_GO, ta_go)
		| REG_FLD_VAL(FLD_TA_SURE, ta_sure)
		| REG_FLD_VAL(FLD_TA_GET, ta_get)
		| REG_FLD_VAL(FLD_DA_HS_EXIT, da_hs_exit);

	if (handle)
		cmdq_pkt_write((struct cmdq_pkt *)handle, comp->cmdq_base,
			comp->regs_pa+DSI_PHY_TIMECON1, value, ~0);
	else
		writel(value, dsi->regs + DSI_PHY_TIMECON1);

	value = REG_FLD_VAL(FLD_CONT_DET, cont_det)
		| REG_FLD_VAL(FLD_DA_HS_SYNC, da_hs_sync)
		| REG_FLD_VAL(FLD_CLK_HS_ZERO, clk_zero)
		| REG_FLD_VAL(FLD_CLK_HS_TRAIL, clk_trail);

	if (handle)
		cmdq_pkt_write((struct cmdq_pkt *)handle, comp->cmdq_base,
			comp->regs_pa+DSI_PHY_TIMECON2, value, ~0);
	else
		writel(value, dsi->regs + DSI_PHY_TIMECON2);

	value = REG_FLD_VAL(FLD_CLK_HS_PREP, clk_hs_prpr)
		| REG_FLD_VAL(FLD_CLK_HS_POST, clk_hs_post)
		| REG_FLD_VAL(FLD_CLK_HS_EXIT, clk_hs_exit);

	if (handle)
		cmdq_pkt_write((struct cmdq_pkt *)handle, comp->cmdq_base,
			comp->regs_pa+DSI_PHY_TIMECON3, value, ~0);
	else
		writel(value, dsi->regs + DSI_PHY_TIMECON3);
}

static void mtk_dsi_cphy_timconfig(struct mtk_dsi *dsi, void *handle)
{

	struct mtk_dsi_phy_timcon *phy_timcon = NULL;
	u32 lpx = 0, hs_prpr = 0, hs_zero = 0, hs_trail = 0;
	u32 ta_get = 0, ta_sure = 0, ta_go = 0, da_hs_exit = 0;
	u32 clk_zero = 0, clk_trail = 0, da_hs_sync = 0;
	u32 clk_hs_prpr = 0, clk_hs_exit = 0, clk_hs_post = 0;
	u32 ui = 0, cycle_time = 0;
	u32 value = 0;
	struct mtk_ddp_comp *comp = &dsi->ddp_comp;

	DDPINFO("%s+\n", __func__);
	ui = 1000 / dsi->data_rate + 0x01;
	cycle_time = 8000 / dsi->data_rate + 0x01;

	lpx = NS_TO_CYCLE(dsi->data_rate * 0x4B, 0x1B58) + 0x1;
	hs_prpr = NS_TO_CYCLE(NS_TO_CYCLE(dsi->data_rate, 2) * 101,
		0x1B58) + 0x1;
	hs_zero = 0x30;
	hs_trail = 0x20;

	ta_get = 5 * NS_TO_CYCLE(0x55, cycle_time);
	ta_sure = 3 * NS_TO_CYCLE(0x55, cycle_time) / 2;
	ta_go = 4 * NS_TO_CYCLE(0x55, cycle_time);
	da_hs_exit = NS_TO_CYCLE(NS_TO_CYCLE(dsi->data_rate, 2) * 225,
		0x1B58) + 0x1;

	clk_zero = NS_TO_CYCLE(0x190, cycle_time);
	clk_trail = NS_TO_CYCLE(0x60, cycle_time) + 0x1;
	da_hs_sync = 0x1;

	clk_hs_prpr = NS_TO_CYCLE(0x40, cycle_time);
	clk_hs_exit = 2 * lpx;
	clk_hs_post = NS_TO_CYCLE(0x60 + 0x34 * ui, cycle_time);

	if (!(dsi->ext && dsi->ext->params))
		goto CONFIG_REG;

	phy_timcon = &dsi->ext->params->phy_timcon;

	lpx = CHK_SWITCH(phy_timcon->lpx, lpx);
	hs_prpr = CHK_SWITCH(phy_timcon->hs_prpr, hs_prpr);
	hs_zero = CHK_SWITCH(phy_timcon->hs_zero, hs_zero);
	hs_trail = CHK_SWITCH(phy_timcon->hs_trail, hs_trail);

	ta_get = CHK_SWITCH(phy_timcon->ta_get, ta_get);
	ta_sure = CHK_SWITCH(phy_timcon->ta_sure, ta_sure);
	ta_go = CHK_SWITCH(phy_timcon->ta_go, ta_go);
	da_hs_exit = CHK_SWITCH(phy_timcon->da_hs_exit, da_hs_exit);

	clk_zero = CHK_SWITCH(phy_timcon->clk_zero, clk_zero);
	clk_trail = CHK_SWITCH(phy_timcon->clk_trail, clk_trail);
	da_hs_sync = CHK_SWITCH(phy_timcon->da_hs_sync, da_hs_sync);

	clk_hs_prpr = CHK_SWITCH(phy_timcon->clk_hs_prpr, clk_hs_prpr);
	clk_hs_exit = CHK_SWITCH(phy_timcon->clk_hs_exit, clk_hs_exit);
	clk_hs_post = CHK_SWITCH(phy_timcon->clk_hs_post, clk_hs_post);

CONFIG_REG:
	dsi->data_phy_cycle = hs_prpr + hs_zero + da_hs_exit + lpx + 5;

	value = REG_FLD_VAL(FLD_LPX, lpx)
		| REG_FLD_VAL(FLD_HS_PREP, hs_prpr)
		| REG_FLD_VAL(FLD_HS_ZERO, hs_zero)
		| REG_FLD_VAL(FLD_HS_TRAIL, hs_trail);

	if (handle)
		cmdq_pkt_write((struct cmdq_pkt *)handle, comp->cmdq_base,
			comp->regs_pa+DSI_PHY_TIMECON0, value, ~0);
	else
		writel(value, dsi->regs + DSI_PHY_TIMECON0);

	value = REG_FLD_VAL(FLD_TA_GO, ta_go)
		| REG_FLD_VAL(FLD_TA_SURE, ta_sure)
		| REG_FLD_VAL(FLD_TA_GET, ta_get)
		| REG_FLD_VAL(FLD_DA_HS_EXIT, da_hs_exit);
	if (handle)
		cmdq_pkt_write((struct cmdq_pkt *)handle, comp->cmdq_base,
			comp->regs_pa+DSI_PHY_TIMECON1, value, ~0);
	else
		writel(value, dsi->regs + DSI_PHY_TIMECON1);

	value = REG_FLD_VAL(FLD_DA_HS_SYNC, da_hs_sync)
		| REG_FLD_VAL(FLD_CLK_HS_ZERO, clk_zero)
		| REG_FLD_VAL(FLD_CLK_HS_TRAIL, clk_trail);
	if (handle)
		cmdq_pkt_write((struct cmdq_pkt *)handle, comp->cmdq_base,
			comp->regs_pa+DSI_PHY_TIMECON2, value, ~0);
	else
		writel(value, dsi->regs + DSI_PHY_TIMECON2);

	value = REG_FLD_VAL(FLD_CLK_HS_PREP, clk_hs_prpr)
		| REG_FLD_VAL(FLD_CLK_HS_POST, clk_hs_post)
		| REG_FLD_VAL(FLD_CLK_HS_EXIT, clk_hs_exit);

	if (handle)
		cmdq_pkt_write((struct cmdq_pkt *)handle, comp->cmdq_base,
			comp->regs_pa+DSI_PHY_TIMECON3, value, ~0);
	else
		writel(value, dsi->regs + DSI_PHY_TIMECON3);
	if (handle)
		cmdq_pkt_write((struct cmdq_pkt *)handle, comp->cmdq_base,
			comp->regs_pa+DSI_PHY_TIMECON0, 0x012c003, ~0);
	else
		writel(0x012c0003, dsi->regs + DSI_CPHY_CON0);
}

static void mtk_dsi_phy_timconfig(struct mtk_dsi *dsi,
		struct cmdq_pkt *handle)
{
	dsi->ext = find_panel_ext(dsi->panel);
	if (!dsi->ext)
		return;

	if (dsi->ext->params->is_cphy)
		mtk_dsi_cphy_timconfig(dsi, handle);
	else
		mtk_dsi_dphy_timconfig(dsi, handle);
}

static void mtk_dsi_enable(struct mtk_dsi *dsi)
{
	mtk_dsi_mask(dsi, DSI_CON_CTRL, DSI_EN, DSI_EN);
#if !defined(CONFIG_MACH_MT6885)
	mtk_dsi_mask(dsi, DSI_CON_CTRL, DSI_CM_WAIT_FIFO_FULL_EN,
		DSI_CM_WAIT_FIFO_FULL_EN);
#endif
}

static void mtk_dsi_disable(struct mtk_dsi *dsi)
{
	mtk_dsi_mask(dsi, DSI_CON_CTRL, DSI_EN, 0);
}

static void mtk_dsi_reset_engine(struct mtk_dsi *dsi)
{
	mtk_dsi_mask(dsi, DSI_CON_CTRL, DSI_RESET, DSI_RESET);
	mtk_dsi_mask(dsi, DSI_CON_CTRL, DSI_RESET, 0);
}

static void mtk_dsi_phy_reset(struct mtk_dsi *dsi)
{
	mtk_dsi_mask(dsi, DSI_CON_CTRL, DSI_PHY_RESET, DSI_PHY_RESET);
	mtk_dsi_mask(dsi, DSI_CON_CTRL, DSI_PHY_RESET, 0);
}

static void mtk_dsi_clear_rxrd_irq(struct mtk_dsi *dsi)
{
	mtk_dsi_mask(dsi, DSI_INTSTA, LPRX_RD_RDY_INT_FLAG, 0);
}
static unsigned int mtk_dsi_default_rate(struct mtk_dsi *dsi)
{
	u32 data_rate;

	/**
	 * vm.pixelclock is in kHz, pixel_clock unit is Hz, so multiply by 1000
	 * htotal_time = htotal * byte_per_pixel / num_lanes
	 * overhead_time = lpx + hs_prepare + hs_zero + hs_trail + hs_exit
	 * mipi_ratio = (htotal_time + overhead_time) / htotal_time
	 * data_rate = pixel_clock * bit_per_pixel * mipi_ratio / num_lanes;
	 */

	if (dsi->ext && dsi->ext->params->data_rate) {
		data_rate = dsi->ext->params->data_rate;
	} else if (dsi->ext && dsi->ext->params->pll_clk) {
		data_rate = dsi->ext->params->pll_clk * 2;
	} else {
		u64 pixel_clock, total_bits;
		u32 htotal, htotal_bits, bit_per_pixel;
		u32 overhead_cycles, overhead_bits;

		switch (dsi->format) {
		case MIPI_DSI_FMT_RGB565:
			bit_per_pixel = 16;
			break;
		case MIPI_DSI_FMT_RGB666_PACKED:
			bit_per_pixel = 18;
			break;
		case MIPI_DSI_FMT_RGB666:
		case MIPI_DSI_FMT_RGB888:
		default:
			bit_per_pixel = 24;
			break;
		}

		pixel_clock = dsi->vm.pixelclock * 1000;
		htotal = dsi->vm.hactive + dsi->vm.hback_porch +
			dsi->vm.hfront_porch + dsi->vm.hsync_len;
		htotal_bits = htotal * bit_per_pixel;

		overhead_cycles = T_LPX + T_HS_PREP + T_HS_ZERO + T_HS_TRAIL +
				T_HS_EXIT;
		overhead_bits = overhead_cycles * dsi->lanes * 8;
		total_bits = htotal_bits + overhead_bits;

		data_rate = DIV_ROUND_UP_ULL(pixel_clock * total_bits,
						  htotal * dsi->lanes);
		data_rate /= 1000000;
	}

	return data_rate;
}
static int mtk_dsi_set_LFR(struct mtk_dsi *dsi, struct mtk_ddp_comp *comp,
	void *handle)
{
	u32 val = 0, mask = 0;

	//lfr_dbg: setting value form debug mode
	unsigned int lfr_dbg = mtk_dbg_get_lfr_dbg_value();
	unsigned int lfr_mode = LFR_MODE_BOTH_MODE;
	unsigned int lfr_type = 0;
	unsigned int lfr_enable = 1;
	unsigned int lfr_skip_num = 0;

	if (mtk_dsi_is_cmd_mode(&dsi->ddp_comp))
		return -1;

	//Settings lfr settings to LFR_CON_REG
	if (dsi->ext && dsi->ext->params &&
		dsi->ext->params->dyn_fps.lfr_minimum_fps != 0 &&
		dsi->ext->params->dyn_fps.lfr_enable == 1) {
		lfr_skip_num =
			(dsi->ext->params->dyn_fps.vact_timing_fps /
			dsi->ext->params->dyn_fps.lfr_minimum_fps) - 1;
	}

	if (lfr_dbg) {
		lfr_mode = mtk_dbg_get_lfr_mode_value();
		lfr_type = mtk_dbg_get_lfr_type_value();
		lfr_enable = mtk_dbg_get_lfr_enable_value();
		lfr_skip_num = mtk_dbg_get_lfr_skip_num_value();
	}


	SET_VAL_MASK(val, mask, lfr_mode, LFR_CON_FLD_REG_LFR_MODE);
	SET_VAL_MASK(val, mask, lfr_type, LFR_CON_FLD_REG_LFR_TYPE);
	SET_VAL_MASK(val, mask, lfr_enable, LFR_CON_FLD_REG_LFR_EN);
	SET_VAL_MASK(val, mask, 0, LFR_CON_FLD_REG_LFR_UPDATE);
	SET_VAL_MASK(val, mask, 1, LFR_CON_FLD_REG_LFR_VSE_DIS);
	SET_VAL_MASK(val, mask, lfr_skip_num, LFR_CON_FLD_REG_LFR_SKIP_NUM);

	if (handle == NULL)
		mtk_dsi_mask(dsi, DSI_LFR_CON, mask, val);

	else
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DSI_LFR_CON, val, mask);

	return 0;
}

static int mtk_dsi_LFR_update(struct mtk_dsi *dsi, struct mtk_ddp_comp *comp,
	void *handle)
{
	u32 val = 0, mask = 0;

	if (mtk_dsi_is_cmd_mode(&dsi->ddp_comp))
		return -1;

	if (comp == NULL) {
		DDPPR_ERR("%s mtk_ddp_comp is null\n", __func__);
		return -1;
	}

	if (handle == NULL) {
		DDPPR_ERR("%s cmdq handle is null\n", __func__);
		return -1;
	}

	SET_VAL_MASK(val, mask, 0, LFR_CON_FLD_REG_LFR_UPDATE);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DSI_LFR_CON, val, mask);

	SET_VAL_MASK(val, mask, 1, LFR_CON_FLD_REG_LFR_UPDATE);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DSI_LFR_CON, val, mask);
	return 0;
}
static int mtk_dsi_LFR_status_check(struct mtk_dsi *dsi)
{
	u32 dsi_LFR_sta;
	u32 dsi_LFR_skip_cnt;
	u32 data;

	data = readl(dsi->regs + DSI_LFR_STA);
	dsi_LFR_sta = REG_FLD_VAL_GET(LFR_STA_FLD_REG_LFR_SKIP_STA, data);
	dsi_LFR_skip_cnt = REG_FLD_VAL_GET(LFR_STA_FLD_REG_LFR_SKIP_CNT, data);
	DDPINFO("%s dsi_LFR_sta=%d, dsi_LFR_skip_cnt=%d\n",
		__func__, dsi_LFR_sta, dsi_LFR_skip_cnt);
	return 0;
}

static int mtk_dsi_set_data_rate(struct mtk_dsi *dsi)
{
	unsigned int data_rate;
	unsigned long mipi_tx_rate;
	int ret = 0;

	data_rate = mtk_dsi_default_rate(dsi);
	mipi_tx_rate = data_rate * 1000000;

	/* Store DSI data rate in MHz */
	dsi->data_rate = data_rate;

	DDPDBG("set mipitx's data rate: %lu Hz\n", mipi_tx_rate);
	ret = clk_set_rate(dsi->hs_clk, mipi_tx_rate);
	return ret;
}

static int mtk_dsi_poweron(struct mtk_dsi *dsi)
{
	struct device *dev = dsi->dev;
	int ret;

	DDPDBG("%s+\n", __func__);
	if (++dsi->clk_refcnt != 1)
		return 0;

	ret = mtk_dsi_set_data_rate(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set data rate: %d\n", ret);
		goto err_refcount;
	}

	if (dsi->ext) {
		if (dsi->ext->params->is_cphy)
			mtk_mipi_tx_cphy_lane_config(dsi->phy, dsi->ext);
		else
			mtk_mipi_tx_dphy_lane_config(dsi->phy, dsi->ext);
	}
	phy_power_on(dsi->phy);

	ret = clk_prepare_enable(dsi->engine_clk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable engine clock: %d\n", ret);
		goto err_phy_power_off;
	}

	ret = clk_prepare_enable(dsi->digital_clk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable digital clock: %d\n", ret);
		goto err_disable_engine_clk;
	}

	if (dsi->ext && dsi->ext->params->ssc_disable != 1) {
		ret = mtk_mipi_tx_ssc_enable(dsi->phy, dsi->ext);
		if (ret < 0)
			DDPPR_ERR("Failed to enable ssc: %d\n", ret);
	}

	mtk_dsi_set_LFR(dsi, NULL, NULL);
#if defined(CONFIG_DRM_MTK_SHADOW_REGISTER_SUPPORT)
	if (dsi->driver_data->support_shadow) {
		/* Enable shadow register and read shadow register */
		mtk_dsi_mask(dsi, DSI_SHADOW_DEBUG,
			DSI_BYPASS_SHADOW, 0x0);
	} else {
		/* Bypass shadow register and read shadow register */
		mtk_dsi_mask(dsi, DSI_SHADOW_DEBUG,
			DSI_BYPASS_SHADOW, DSI_BYPASS_SHADOW);
	}
#else
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) \
	|| defined(CONFIG_MACH_MT6833)
	/* Bypass shadow register and read shadow register */
	mtk_dsi_mask(dsi, DSI_SHADOW_DEBUG,
		DSI_BYPASS_SHADOW, DSI_BYPASS_SHADOW);
#endif
#endif

	DDPDBG("%s-\n", __func__);

	return 0;

err_disable_engine_clk:
	clk_disable_unprepare(dsi->engine_clk);
err_phy_power_off:
	phy_power_off(dsi->phy);
err_refcount:
	dsi->clk_refcnt--;
	return ret;
}

static bool mtk_dsi_clk_hs_state(struct mtk_dsi *dsi)
{
	u32 tmp_reg1;

	tmp_reg1 = readl(dsi->regs + DSI_PHY_LCCON);
	return ((tmp_reg1 & LC_HS_TX_EN) == 1) ? true : false;
}

static void mtk_dsi_clk_hs_mode(struct mtk_dsi *dsi, bool enter)
{
	if (enter && !mtk_dsi_clk_hs_state(dsi))
		mtk_dsi_mask(dsi, DSI_PHY_LCCON, LC_HS_TX_EN, LC_HS_TX_EN);
	else if (!enter && mtk_dsi_clk_hs_state(dsi))
		mtk_dsi_mask(dsi, DSI_PHY_LCCON, LC_HS_TX_EN, 0);
}

static void mtk_dsi_set_mode(struct mtk_dsi *dsi)
{
	u32 vid_mode = CMD_MODE;

	if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO) {
		if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_BURST)
			vid_mode = BURST_MODE;
		else if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_SYNC_PULSE)
			vid_mode = SYNC_PULSE_MODE;
		else
			vid_mode = SYNC_EVENT_MODE;
	}
	writel(vid_mode, dsi->regs + DSI_MODE_CTRL);
}

static void mtk_dsi_set_vm_cmd(struct mtk_dsi *dsi)
{
	mtk_dsi_mask(dsi, DSI_VM_CMD_CON, VM_CMD_EN, VM_CMD_EN);
	mtk_dsi_mask(dsi, DSI_VM_CMD_CON, TS_VFP_EN, TS_VFP_EN);
}

static int mtk_dsi_get_virtual_heigh(struct mtk_dsi *dsi,
	struct drm_crtc *crtc)
{
	struct mtk_panel_ext *panel_ext = NULL;
	struct mtk_crtc_state *state =
	    to_mtk_crtc_state(crtc->state);
	struct drm_display_mode adjusted_mode = state->base.adjusted_mode;
	unsigned int virtual_heigh = adjusted_mode.vdisplay;

	panel_ext = dsi->ext;
	if (panel_ext && panel_ext->funcs
			&& panel_ext->funcs->get_virtual_heigh)
		virtual_heigh = panel_ext->funcs->get_virtual_heigh();

	if (!virtual_heigh)
		virtual_heigh = crtc->mode.vdisplay;
	DDPINFO("%s,virtual_heigh %d\n", __func__, virtual_heigh);
	return virtual_heigh;
}

static int mtk_dsi_get_virtual_width(struct mtk_dsi *dsi,
	struct drm_crtc *crtc)
{
	struct mtk_panel_ext *panel_ext = NULL;
	struct mtk_crtc_state *state =
	    to_mtk_crtc_state(crtc->state);
	struct drm_display_mode adjusted_mode = state->base.adjusted_mode;
	unsigned int virtual_width = adjusted_mode.hdisplay;

	panel_ext = dsi->ext;
	if (panel_ext && panel_ext->funcs
			&& panel_ext->funcs->get_virtual_width)
		virtual_width = panel_ext->funcs->get_virtual_width();
	if (!virtual_width)
		virtual_width = crtc->mode.hdisplay;
	DDPINFO("%s,virtual_width %d\n", __func__, virtual_width);
	return virtual_width;
}

static void mtk_dsi_ps_control_vact(struct mtk_dsi *dsi)
{
	u32 ps_wc, size;
	u32 dsi_buf_bpp, val;
	u32 value = 0, mask = 0;
	u32 width = mtk_dsi_get_virtual_width(dsi, dsi->encoder.crtc);
	u32 height = mtk_dsi_get_virtual_heigh(dsi, dsi->encoder.crtc);
	struct mtk_panel_ext *ext = mtk_dsi_get_panel_ext(&dsi->ddp_comp);
	struct mtk_panel_dsc_params *dsc_params = &ext->params->dsc_params;

	if (dsi->format == MIPI_DSI_FMT_RGB565)
		dsi_buf_bpp = 2;
	else
		dsi_buf_bpp = 3;

	if (dsc_params->enable == 0) {
		ps_wc = width * dsi_buf_bpp;
		SET_VAL_MASK(value, mask, ps_wc, DSI_PS_WC);

		switch (dsi->format) {
		case MIPI_DSI_FMT_RGB888:
			SET_VAL_MASK(value, mask, 3, DSI_PS_SEL);
			break;
		case MIPI_DSI_FMT_RGB666:
			SET_VAL_MASK(value, mask, 2, DSI_PS_SEL);
			break;
		case MIPI_DSI_FMT_RGB666_PACKED:
			SET_VAL_MASK(value, mask, 1, DSI_PS_SEL);
			break;
		case MIPI_DSI_FMT_RGB565:
			SET_VAL_MASK(value, mask, 0, DSI_PS_SEL);
			break;
		}
		size = (height << 16) + width;
	} else {
		ps_wc = (((dsc_params->chunk_size + 2) / 3) * 3);
		if (dsc_params->slice_mode == 1)
			ps_wc *= 2;

		SET_VAL_MASK(value, mask, ps_wc, DSI_PS_WC);
		SET_VAL_MASK(value, mask, 5, DSI_PS_SEL);

		size = (height << 16) + (ps_wc / 3);
	}

	writel(height, dsi->regs + DSI_VACT_NL);

	val = readl(dsi->regs + DSI_PSCTRL);
	val = (val & ~mask) | (value & mask);
	writel(val, dsi->regs + DSI_PSCTRL);

#if !defined(CONFIG_MACH_MT6885) && !defined(CONFIG_MACH_MT6873) \
	&& !defined(CONFIG_MACH_MT6893) && !defined(CONFIG_MACH_MT6853) \
	&& !defined(CONFIG_MACH_MT6833)
	val = vm->hactive * dsi_buf_bpp;
	writel(val, dsi->regs + DSI_HSTX_CKL_WC);
#endif

	writel(size, dsi->regs + DSI_SIZE_CON);
}

static void mtk_dsi_rxtx_control(struct mtk_dsi *dsi)
{
	u32 tmp_reg;

	switch (dsi->lanes) {
	case 1:
		tmp_reg = 1 << 2;
		break;
	case 2:
		tmp_reg = 3 << 2;
		break;
	case 3:
		tmp_reg = 7 << 2;
		break;
	case 4:
		tmp_reg = 0xf << 2;
		break;
	default:
		tmp_reg = 0xf << 2;
		break;
	}

	tmp_reg |= (dsi->mode_flags & MIPI_DSI_CLOCK_NON_CONTINUOUS) << 6;
#if !defined(CONFIG_MACH_MT6885) && !defined(CONFIG_MACH_MT6873) \
	&& !defined(CONFIG_MACH_MT6893) && !defined(CONFIG_MACH_MT6853) \
	&& !defined(CONFIG_MACH_MT6833)
	tmp_reg |= (dsi->mode_flags & MIPI_DSI_MODE_EOT_PACKET) >> 3;
#endif

	writel(tmp_reg, dsi->regs + DSI_TXRX_CTRL);

	/* need to config for cmd mode to transmit frame data to DDIC */
	writel(DSI_WMEM_CONTI, dsi->regs + DSI_MEM_CONTI);
}

static void mtk_dsi_calc_vdo_timing(struct mtk_dsi *dsi)
{
	u32 horizontal_sync_active_byte;
	u32 horizontal_backporch_byte;
	u32 horizontal_frontporch_byte;
	u32 dsi_tmp_buf_bpp;
	u32 t_vfp, t_vbp, t_vsa;
	u32 t_hfp, t_hbp, t_hsa;
	struct mtk_panel_ext *ext = dsi->ext;
	struct videomode *vm = &dsi->vm;
	struct dynamic_mipi_params *dyn = NULL;

	if (ext && ext->params)
		dyn = &ext->params->dyn;

	t_vfp = (dsi->mipi_hopping_sta) ?
			((dyn && !!dyn->vfp) ?
			 dyn->vfp : vm->vfront_porch) :
			vm->vfront_porch;

	t_vbp = (dsi->mipi_hopping_sta) ?
			((dyn && !!dyn->vbp) ?
			 dyn->vbp : vm->vback_porch) :
			vm->vback_porch;

	t_vsa = (dsi->mipi_hopping_sta) ?
			((dyn && !!dyn->vsa) ?
			 dyn->vsa : vm->vsync_len) :
			vm->vsync_len;

	t_hfp = (dsi->mipi_hopping_sta) ?
			((dyn && !!dyn->hfp) ?
			 dyn->hfp : vm->hfront_porch) :
			vm->hfront_porch;

	t_hbp = (dsi->mipi_hopping_sta) ?
			((dyn && !!dyn->hbp) ?
			 dyn->hbp : vm->hback_porch) :
			vm->hback_porch;

	t_hsa = (dsi->mipi_hopping_sta) ?
			((dyn && !!dyn->hsa) ?
			 dyn->hsa : vm->hsync_len) :
			vm->hsync_len;

	if (dsi->format == MIPI_DSI_FMT_RGB565)
		dsi_tmp_buf_bpp = 2;
	else
		dsi_tmp_buf_bpp = 3;

	dsi->ext = find_panel_ext(dsi->panel);
	if (!dsi->ext)
		return;

	if (dsi->ext->params->is_cphy) {
		if (t_hsa * dsi_tmp_buf_bpp < 10 * dsi->lanes + 26 + 5)
			horizontal_sync_active_byte = 4;
		else
			horizontal_sync_active_byte = ALIGN_TO(
				t_hsa * dsi_tmp_buf_bpp -
				10 * dsi->lanes - 26, 2);

		if (t_hbp * dsi_tmp_buf_bpp < 12 * dsi->lanes + 26 + 5)
			horizontal_backporch_byte = 4;
		else
			horizontal_backporch_byte = ALIGN_TO(
				t_hbp * dsi_tmp_buf_bpp -
				12 * dsi->lanes - 26, 2);

		if (t_hfp * dsi_tmp_buf_bpp < 8 * dsi->lanes + 28 +
			2 * dsi->data_phy_cycle * dsi->lanes + 9)
			horizontal_frontporch_byte = 8;
		else if ((t_hfp * dsi_tmp_buf_bpp > 8 * dsi->lanes + 28 +
			2 * dsi->data_phy_cycle * dsi->lanes + 8) &&
			(t_hfp * dsi_tmp_buf_bpp < 8 * dsi->lanes + 28 +
			2 * dsi->data_phy_cycle * dsi->lanes +
			2 * (32 + 1) * dsi->lanes - 6 * dsi->lanes - 12))
			horizontal_frontporch_byte = 2*(32 + 1)*dsi->lanes -
				6*dsi->lanes - 12;
		else
			horizontal_frontporch_byte = t_hfp * dsi_tmp_buf_bpp -
				8 * dsi->lanes - 28 -
				2 * dsi->data_phy_cycle * dsi->lanes;
	} else {
		if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_SYNC_PULSE) {
			horizontal_sync_active_byte =
				ALIGN_TO((t_hsa * dsi_tmp_buf_bpp - 10), 4);

			horizontal_backporch_byte =
				ALIGN_TO((t_hbp * dsi_tmp_buf_bpp - 10), 4);
		} else {
			horizontal_sync_active_byte =
				ALIGN_TO((t_hsa * dsi_tmp_buf_bpp - 4), 4);

			horizontal_backporch_byte =
				ALIGN_TO(((t_hbp + t_hsa) * dsi_tmp_buf_bpp -
				 10), 4);
		}

		horizontal_frontporch_byte =
			ALIGN_TO((t_hfp * dsi_tmp_buf_bpp - 12), 4);
	}
	dsi->vfp = t_vfp;
	dsi->vbp = t_vbp;
	dsi->vsa = t_vsa;
	dsi->hfp_byte = horizontal_frontporch_byte;
	dsi->hbp_byte = horizontal_backporch_byte;
	dsi->hsa_byte = horizontal_sync_active_byte;
}

static void mtk_dsi_config_vdo_timing(struct mtk_dsi *dsi)
{
	struct videomode *vm = &dsi->vm;
	unsigned int vact = vm->vactive;

	writel(dsi->vsa, dsi->regs + DSI_VSA_NL);
	writel(dsi->vbp, dsi->regs + DSI_VBP_NL);
	writel(dsi->vfp, dsi->regs + DSI_VFP_NL);
	vact = mtk_dsi_get_virtual_heigh(dsi, dsi->encoder.crtc);
	writel(vact, dsi->regs + DSI_VACT_NL);

	writel(dsi->hsa_byte, dsi->regs + DSI_HSA_WC);
	writel(dsi->hbp_byte, dsi->regs + DSI_HBP_WC);
	writel(dsi->hfp_byte, dsi->regs + DSI_HFP_WC);
}

static void mtk_dsi_start(struct mtk_dsi *dsi)
{
	writel(0, dsi->regs + DSI_START);
	writel(1, dsi->regs + DSI_START);
}

static void mtk_dsi_vm_start(struct mtk_dsi *dsi)
{
	mtk_dsi_mask(dsi, DSI_START, VM_CMD_START, 0);
	mtk_dsi_mask(dsi, DSI_START, VM_CMD_START, VM_CMD_START);
}

static void mtk_dsi_stop(struct mtk_dsi *dsi)
{
	writel(0, dsi->regs + DSI_START);
	writel(0, dsi->regs + DSI_INTEN);
	writel(0, dsi->regs + DSI_INTSTA);
}

static void mtk_dsi_set_interrupt_enable(struct mtk_dsi *dsi)
{
	u32 inten;

	inten = BUFFER_UNDERRUN_INT_FLAG | INP_UNFINISH_INT_EN;

	if (!mtk_dsi_is_cmd_mode(&dsi->ddp_comp))
		inten |= FRAME_DONE_INT_FLAG;
	else
		inten |= TE_RDY_INT_FLAG;

	writel(inten, dsi->regs + DSI_INTEN);
}

static void mtk_dsi_irq_data_set(struct mtk_dsi *dsi, u32 irq_bit)
{
	dsi->irq_data |= irq_bit;
}

static void mtk_dsi_irq_data_clear(struct mtk_dsi *dsi, u32 irq_bit)
{
	dsi->irq_data &= ~irq_bit;
}

static s32 mtk_dsi_wait_for_irq_done(struct mtk_dsi *dsi, u32 irq_flag,
				     unsigned int timeout)
{
	s32 ret = 0;

	unsigned long jiffies = msecs_to_jiffies(timeout);

	ret = wait_event_interruptible_timeout(
		dsi->irq_wait_queue, dsi->irq_data & irq_flag, jiffies);
	if (ret == 0) {
		DRM_WARN("Wait DSI IRQ(0x%08x) Timeout\n", irq_flag);

		mtk_dsi_enable(dsi);
		mtk_dsi_reset_engine(dsi);
	}
	return ret;
}

static void mtk_dsi_cmdq_poll(struct mtk_ddp_comp *comp,
			      struct cmdq_pkt *handle, unsigned int reg,
			      unsigned int val, unsigned int mask)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct cmdq_client *client = mtk_crtc->gce_obj.client[CLIENT_DSI_CFG];

	if (handle == NULL)
		DDPPR_ERR("%s no cmdq handle\n", __func__);

#if 0
	cmdq_pkt_poll_reg(handle, val, comp->cmdq_subsys, reg & 0xFFFF, mask);
#else
	if (handle->cl == (void *)client) {
		cmdq_pkt_poll_timeout(handle, val, SUBSYS_NO_SUPPORT,
					  reg, mask, 0xFFFF,
					  CMDQ_GPR_R14);
	} else {
		cmdq_pkt_poll_timeout(handle, val, SUBSYS_NO_SUPPORT,
					  reg, mask, 0xFFFF,
					  CMDQ_GPR_R07);
	}
#endif
}

static s32 mtk_dsi_poll_for_idle(struct mtk_dsi *dsi, struct cmdq_pkt *handle)
{
	unsigned int loop_cnt = 0;
	s32 tmp;

	if (handle) {
		mtk_dsi_cmdq_poll(&dsi->ddp_comp, handle,
				  dsi->ddp_comp.regs_pa + DSI_INTSTA, 0,
				  0x80000000);
		return 1;
	}

	while (loop_cnt < 100 * 1000) {
		tmp = readl(dsi->regs + DSI_INTSTA);
		if (!(tmp & DSI_BUSY))
			return 1;
		loop_cnt++;
		udelay(1);
	}
	DDPPR_ERR("%s timeout\n", __func__);
	return 0;
}

static s32 mtk_dsi_wait_idle(struct mtk_dsi *dsi, u32 irq_flag,
			     unsigned int timeout, struct cmdq_pkt *handle)
{

	if (dsi->driver_data->poll_for_idle)
		return dsi->driver_data->poll_for_idle(dsi, handle);

	return mtk_dsi_wait_for_irq_done(dsi, irq_flag, timeout);
}

static void init_dsi_wq(struct mtk_dsi *dsi)
{
	init_waitqueue_head(&dsi->enter_ulps_done.wq);
	init_waitqueue_head(&dsi->exit_ulps_done.wq);
	init_waitqueue_head(&dsi->te_rdy.wq);
	init_waitqueue_head(&dsi->frame_done.wq);

	atomic_set(&dsi->enter_ulps_done.condition, 0);
	atomic_set(&dsi->exit_ulps_done.condition, 0);
	atomic_set(&dsi->te_rdy.condition, 0);
	atomic_set(&dsi->frame_done.condition, 0);
}

static void reset_dsi_wq(struct t_condition_wq *wq)
{
	atomic_set(&wq->condition, 0);
}

static void wakeup_dsi_wq(struct t_condition_wq *wq)
{
	atomic_set(&wq->condition, 1);
	wake_up(&wq->wq);
}

static int wait_dsi_wq(struct t_condition_wq *wq, int timeout)
{
	int ret;

	ret = wait_event_timeout(wq->wq, atomic_read(&wq->condition), timeout);

	atomic_set(&wq->condition, 0);

	return ret;
}

static irqreturn_t mtk_dsi_irq_status(int irq, void *dev_id)
{
	struct mtk_dsi *dsi = dev_id;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_panel_ext *panel_ext;
	u32 status;
	static unsigned int dsi_underrun_trigger = 1;
	unsigned int ret = 0;
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) \
	|| defined(CONFIG_MACH_MT6833)
	static DEFINE_RATELIMIT_STATE(ioctl_ratelimit, 1 * HZ, 20);
#endif
	bool doze_enabled = 0;
	unsigned int doze_wait = 0;
	static unsigned int cnt;

	if (mtk_drm_top_clk_isr_get("dsi_irq") == false) {
		DDPIRQ("%s, top clk off\n", __func__);
		return IRQ_NONE;
	}

	status = readl(dsi->regs + DSI_INTSTA);
	if (!status) {
		ret = IRQ_NONE;
		goto out;
	}

	DRM_MMP_MARK(IRQ, irq, status);

	if (dsi->ddp_comp.id == DDP_COMPONENT_DSI0)
		DRM_MMP_MARK(dsi0, status, 0);
	else if (dsi->ddp_comp.id == DDP_COMPONENT_DSI1)
		DRM_MMP_MARK(dsi1, status, 0);

	DDPIRQ("%s irq, val:0x%x\n", mtk_dump_comp_str(&dsi->ddp_comp), status);

	/*
	 * rd_rdy don't clear and wait for ESD &
	 * Read LCM will clear the bit.
	 */
	/* do not clear vm command done */
	status &= 0xffde;
	if (status) {
		writel(~status, dsi->regs + DSI_INTSTA);
		if (status & BUFFER_UNDERRUN_INT_FLAG) {
			struct mtk_drm_private *priv = NULL;

			mtk_crtc = dsi->ddp_comp.mtk_crtc;

			if (mtk_crtc && mtk_crtc->base.dev)
				priv = mtk_crtc->base.dev->dev_private;
			if (priv && mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_DSI_UNDERRUN_AEE)) {
				if (dsi_underrun_trigger == 1) {
					DDPAEE(
						"[IRQ] %s:buffer underrun,sys_time=%u\n",
						mtk_dump_comp_str(
							&dsi->ddp_comp),
						(u32)arch_counter_get_cntvct());
					if (dsi->encoder.crtc) {
						mtk_drm_crtc_analysis(
							dsi->encoder.crtc);
						mtk_drm_crtc_dump(
							dsi->encoder.crtc);
					}
					dsi_underrun_trigger = 0;
				}
			}

#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) \
	|| defined(CONFIG_MACH_MT6833)
			mtk_dprec_logger_pr(DPREC_LOGGER_ERROR,
				"[IRQ] %s: buffer underrun\n",
				mtk_dump_comp_str(&dsi->ddp_comp));
			if (__ratelimit(&ioctl_ratelimit))
				pr_err(pr_fmt("[IRQ] %s: buffer underrun\n"),
					mtk_dump_comp_str(&dsi->ddp_comp));
#else
			DDPPR_ERR("[IRQ] %s: buffer underrun\n",
				mtk_dump_comp_str(&dsi->ddp_comp));
#endif

			if (dsi_underrun_trigger == 1 && dsi->encoder.crtc) {
				mtk_drm_crtc_analysis(dsi->encoder.crtc);
				mtk_drm_crtc_dump(dsi->encoder.crtc);
				dsi_underrun_trigger = 0;
			}
		}

		if (status & INP_UNFINISH_INT_EN)
			DDPPR_ERR("[IRQ] %s: input relay unfinish\n",
				  mtk_dump_comp_str(&dsi->ddp_comp));

		if (status & SLEEPOUT_DONE_INT_FLAG)
			wakeup_dsi_wq(&dsi->exit_ulps_done);

		if (status & SLEEPIN_ULPS_DONE_INT_FLAG)
			wakeup_dsi_wq(&dsi->enter_ulps_done);

		if (status & TE_RDY_INT_FLAG) {
			struct mtk_drm_private *priv = NULL;

			if (dsi->ddp_comp.id == DDP_COMPONENT_DSI0) {
				unsigned long long ext_te_time = sched_clock();

				lcm_fps_ctx_update(ext_te_time, 0, 0);
			}
			mtk_crtc = dsi->ddp_comp.mtk_crtc;

			if (mtk_crtc && mtk_crtc->base.dev)
				priv = mtk_crtc->base.dev->dev_private;
			if (priv && mtk_drm_helper_get_opt(priv->helper_opt,
							   MTK_DRM_OPT_HBM))
				wakeup_dsi_wq(&dsi->te_rdy);

			if (mtk_dsi_is_cmd_mode(&dsi->ddp_comp) &&
				mtk_crtc && mtk_crtc->vblank_en) {
				panel_ext = dsi->ext;

				if (dsi->encoder.crtc)
					doze_enabled = mtk_dsi_doze_state(dsi);

				if (panel_ext->params->doze_delay &&
					doze_enabled) {
					doze_wait =
						panel_ext->params->doze_delay;
					if (cnt % doze_wait == 0) {
						mtk_crtc_vblank_irq(
							&mtk_crtc->base);
						cnt = 0;
					}
					cnt++;
				} else
					mtk_crtc_vblank_irq(&mtk_crtc->base);
			}
		}

		if (status & FRAME_DONE_INT_FLAG) {
			struct mtk_drm_private *priv = NULL;

			mtk_crtc = dsi->ddp_comp.mtk_crtc;

			if (mtk_crtc && mtk_crtc->base.dev)
				priv = mtk_crtc->base.dev->dev_private;
			if (priv && mtk_drm_helper_get_opt(priv->helper_opt,
							   MTK_DRM_OPT_HBM))
				wakeup_dsi_wq(&dsi->frame_done);

			if (!mtk_dsi_is_cmd_mode(&dsi->ddp_comp) &&
				mtk_crtc && mtk_crtc->vblank_en)
				mtk_crtc_vblank_irq(&mtk_crtc->base);
			DDPIRQ("%s frame done!\n", __func__);
		}
	}

	ret = IRQ_HANDLED;

out:
	mtk_drm_top_clk_isr_put("dsi_irq");

	return ret;
}

static irqreturn_t mtk_dsi_irq(int irq, void *dev_id)
{
	struct mtk_dsi *dsi = dev_id;
	u32 status, tmp;
	u32 flag = LPRX_RD_RDY_INT_FLAG | CMD_DONE_INT_FLAG | VM_DONE_INT_FLAG;

	status = readl(dsi->regs + DSI_INTSTA) & flag;
	if (status) {
		do {
			mtk_dsi_mask(dsi, DSI_RACK, RACK, RACK);
			tmp = readl(dsi->regs + DSI_INTSTA);
		} while (tmp & DSI_BUSY);

		mtk_dsi_mask(dsi, DSI_INTSTA, status, 0);
		mtk_dsi_irq_data_set(dsi, status);
		wake_up_interruptible(&dsi->irq_wait_queue);
	}

	return IRQ_HANDLED;
}

static void mtk_dsi_poweroff(struct mtk_dsi *dsi)
{
	DDPDBG("%s +\n", __func__);
	if (dsi->clk_refcnt == 0) {
		DDPAEE("%s:%d, invalid cnt:%d\n",
			__func__, __LINE__,
			dsi->clk_refcnt);
		return;
	}

	if (--dsi->clk_refcnt != 0)
		return;

	if (dsi->ext && dsi->ext->params->ssc_disable != 1)
		mtk_mipi_tx_ssc_disable(dsi->phy);
	clk_disable_unprepare(dsi->engine_clk);
	clk_disable_unprepare(dsi->digital_clk);
	phy_power_off(dsi->phy);
	DDPDBG("%s -\n", __func__);
}

static void mtk_dsi_enter_ulps(struct mtk_dsi *dsi)
{
	unsigned int ret = 0;

	/* reset enter_ulps_done before waiting */
	reset_dsi_wq(&dsi->enter_ulps_done);
	/* config and trigger enter ulps mode */
	mtk_dsi_mask(dsi, DSI_INTEN, SLEEPIN_ULPS_DONE_INT_FLAG,
		     SLEEPIN_ULPS_DONE_INT_FLAG);
	mtk_dsi_mask(dsi, DSI_PHY_LCCON, LC_HS_TX_EN, 0);
	mtk_dsi_mask(dsi, DSI_PHY_LD0CON, LDX_ULPM_AS_L0, LDX_ULPM_AS_L0);
	mtk_dsi_mask(dsi, DSI_PHY_LD0CON, LD0_ULPM_EN, LD0_ULPM_EN);
	mtk_dsi_mask(dsi, DSI_PHY_LCCON, LC_ULPM_EN, LC_ULPM_EN);

	/* wait enter_ulps_done */
	ret = wait_dsi_wq(&dsi->enter_ulps_done, 2 * HZ);

	if (ret)
		DDPDBG("%s success\n", __func__);
	else {
		/* IRQ maybe be un-expectedly disabled for long time,
		 * which makes false alarm timeout...
		 */
		u32 status = readl(dsi->regs + DSI_INTSTA);

		if (status & SLEEPIN_ULPS_DONE_INT_FLAG)
			DDPPR_ERR("%s success but IRQ is blocked\n",
				__func__);
		else {
			mtk_dsi_dump(&dsi->ddp_comp);
			DDPAEE("%s fail\n", __func__);
		}
	}

	mtk_dsi_reset_engine(dsi);
	mtk_dsi_lane0_ulp_mode_enter(dsi);
	mtk_dsi_clk_ulp_mode_enter(dsi);
	/* set the lane number as 0 to pull down mipi */
	writel(0, dsi->regs + DSI_TXRX_CTRL);

	mtk_mipi_tx_pre_oe_config(dsi->phy, 0);
	mtk_mipi_tx_sw_control_en(dsi->phy, 1);

	/* set lane num = 0 */
	mtk_dsi_mask(dsi, DSI_TXRX_CTRL, LANE_NUM, 0);

}

static void mtk_dsi_exit_ulps(struct mtk_dsi *dsi)
{
	int wake_up_prd = (dsi->data_rate * 1000) / (1024 * 8) + 1;
	unsigned int ret = 0;

	mtk_dsi_phy_reset(dsi);
	/* set pre oe */
	mtk_mipi_tx_pre_oe_config(dsi->phy, 1);

	/* reset exit_ulps_done before waiting */
	reset_dsi_wq(&dsi->exit_ulps_done);

	mtk_dsi_mask(dsi, DSI_INTEN, SLEEPOUT_DONE_INT_FLAG,
		     SLEEPOUT_DONE_INT_FLAG);
	mtk_dsi_mask(dsi, DSI_PHY_LD0CON, LDX_ULPM_AS_L0, LDX_ULPM_AS_L0);
	mtk_dsi_mask(dsi, DSI_MODE_CTRL, SLEEP_MODE, SLEEP_MODE);
	mtk_dsi_mask(dsi, DSI_TIME_CON0, 0xffff, wake_up_prd);

	/* free sw control */
	mtk_mipi_tx_sw_control_en(dsi->phy, 0);

	mtk_dsi_mask(dsi, DSI_START, SLEEPOUT_START, 0);
	mtk_dsi_mask(dsi, DSI_START, SLEEPOUT_START, SLEEPOUT_START);

	/* wait exit_ulps_done */
	ret = wait_dsi_wq(&dsi->exit_ulps_done, 2 * HZ);

	if (ret)
		DDPDBG("%s success\n", __func__);
	else {
		/* IRQ maybe be un-expectedly disabled for long time,
		 * which makes false alarm timeout...
		 */
		u32 status = readl(dsi->regs + DSI_INTSTA);

		if (status & SLEEPOUT_DONE_INT_FLAG)
			DDPPR_ERR("%s success but IRQ is blocked\n",
				__func__);
		else {
			mtk_dsi_dump(&dsi->ddp_comp);
			DDPAEE("%s fail\n", __func__);
		}
	}

	/* reset related setting */
	mtk_dsi_mask(dsi, DSI_INTEN, SLEEPOUT_DONE_INT_FLAG, 0);
	mtk_dsi_mask(dsi, DSI_PHY_LD0CON, LDX_ULPM_AS_L0, 0);
	mtk_dsi_mask(dsi, DSI_MODE_CTRL, SLEEP_MODE, 0);
	mtk_dsi_mask(dsi, DSI_START, SLEEPOUT_START, 0);

	/* do DSI reset after exit ULPS */
	mtk_dsi_reset_engine(dsi);
}

static int mtk_dsi_stop_vdo_mode(struct mtk_dsi *dsi, void *handle);

static void mipi_dsi_dcs_write_gce2(struct mtk_dsi *dsi, struct cmdq_pkt *dummy,
					  const void *data, size_t len);

static void mtk_output_en_doze_switch(struct mtk_dsi *dsi)
{
	bool doze_enabled = mtk_dsi_doze_state(dsi);
	struct mtk_panel_funcs *panel_funcs;

	if (!dsi->output_en)
		return;

	DDPINFO("%s doze_enabled state change %d->%d\n", __func__,
		dsi->doze_enabled, doze_enabled);

	if (dsi->ext && dsi->ext->funcs) {
		panel_funcs = dsi->ext->funcs;
	} else {
		DDPINFO("%s, AOD should have use panel extension function\n",
			__func__);
		return;
	}

	/* Change LCM Doze mode */
	if (doze_enabled && panel_funcs->doze_enable_start)
		panel_funcs->doze_enable_start(dsi->panel, dsi,
			mipi_dsi_dcs_write_gce2, NULL);
	else if (!doze_enabled && panel_funcs->doze_disable)
		panel_funcs->doze_disable(dsi->panel, dsi,
			mipi_dsi_dcs_write_gce2, NULL);

	/* Display mode switch */
	if (panel_funcs->doze_get_mode_flags) {
		if (!mtk_dsi_is_cmd_mode(&dsi->ddp_comp))
			mtk_dsi_stop_vdo_mode(dsi, NULL);

		/* set DSI into ULPS mode */
		mtk_dsi_reset_engine(dsi);

		dsi->mode_flags =
			panel_funcs->doze_get_mode_flags(
				dsi->panel, doze_enabled);

		if (mtk_dsi_is_cmd_mode(&dsi->ddp_comp))
			writel(0x0001023c, dsi->regs + DSI_TXRX_CTRL);

		mtk_dsi_set_mode(dsi);
		mtk_dsi_clk_hs_mode(dsi, 1);

		/* Update RDMA golden setting after switch */
		{
			struct drm_crtc *crtc = dsi->encoder.crtc;
			struct mtk_drm_crtc *mtk_crtc =
			    to_mtk_crtc(dsi->encoder.crtc);
			unsigned int i, j;
			struct cmdq_pkt *handle;
			struct mtk_ddp_comp *comp;
			struct mtk_ddp_config cfg;

			mtk_crtc_pkt_create(&handle, &mtk_crtc->base,
				mtk_crtc->gce_obj.client[CLIENT_CFG]);
			cfg.w = crtc->state->adjusted_mode.hdisplay;
			cfg.h = crtc->state->adjusted_mode.vdisplay;
			cfg.vrefresh = crtc->state->adjusted_mode.vrefresh;
			cfg.bpc = mtk_crtc->bpc;
			cfg.p_golden_setting_context =
				__get_golden_setting_context(mtk_crtc);
			for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
				mtk_ddp_comp_io_cmd(comp, handle,
					MTK_IO_CMD_RDMA_GOLDEN_SETTING, &cfg);
			cmdq_pkt_flush(handle);
			cmdq_pkt_destroy(handle);
		}

		if (!mtk_dsi_is_cmd_mode(&dsi->ddp_comp)) {
			mtk_dsi_set_vm_cmd(dsi);
			mtk_dsi_calc_vdo_timing(dsi);
			mtk_dsi_config_vdo_timing(dsi);
			mtk_dsi_start(dsi);
		}
	}

	if (doze_enabled && panel_funcs->doze_enable)
		panel_funcs->doze_enable(dsi->panel, dsi,
			mipi_dsi_dcs_write_gce2, NULL);

	if (doze_enabled && panel_funcs->doze_area)
		panel_funcs->doze_area(dsi->panel, dsi,
			mipi_dsi_dcs_write_gce2, NULL);

	if (panel_funcs->doze_post_disp_on)
		panel_funcs->doze_post_disp_on(dsi->panel,
			dsi, mipi_dsi_dcs_write_gce2, NULL);

	dsi->doze_enabled = doze_enabled;
}

static void mtk_dsi_crtc_notify(struct mtk_dsi *dsi)
{
	unsigned int fod_flag_last_state;

	if (dsi) {
		fod_flag_last_state = dsi->conn.fod_ui_ready;
		dsi->conn.fod_ui_ready &= (~1);

		if (dsi->conn.fod_ui_ready != fod_flag_last_state) {
			pr_info("panel mtk_dsi_crtc_notify fod_ui_ready=%d", dsi->conn.fod_ui_ready);
			sysfs_notify(&dsi->conn.kdev->kobj, NULL, "fod_ui_ready");
		}
	}
	return;
}

static void mtk_output_dsi_enable(struct mtk_dsi *dsi,
	int force_lcm_update)
{
	int ret;
	struct mtk_panel_ext *ext = dsi->ext;
	bool new_doze_state = mtk_dsi_doze_state(dsi);
	struct drm_crtc *crtc = dsi->encoder.crtc;
	struct mtk_crtc_state *mtk_state = to_mtk_crtc_state(crtc->state);
	unsigned int mode_id = mtk_state->prop_val[CRTC_PROP_DISP_MODE_IDX];

	DDPDSIINFO("%s +, new_doze_state = %d, dsi->output_en =%d\n", __func__, new_doze_state, dsi->output_en);
	if (dsi->doze_state == DRM_BLANK_POWERDOWN) {
		dsi->doze_state = DRM_BLANK_UNBLANK;
		pr_info("%s power on from power off\n", __func__);
	}
	//mkt_disp_esd_irq_ctrl(dsi->ddp_comp.mtk_crtc->esd_ctx, true);
	//g_notify_data.data = &dsi->doze_state;

	if (dsi->output_en) {
		if (mtk_dsi_doze_status_change(dsi))
			mtk_output_en_doze_switch(dsi);
		else
			DDPDSIINFO("dsi is initialized\n");
		return;
	}

	//drm_notifier_call_chain(DRM_EARLY_EVENT_BLANK, &g_notify_data);

	ret = mtk_dsi_poweron(dsi);
	if (ret < 0) {
		DDPPR_ERR("failed to power on dsi\n");
		return;
	}

	mtk_dsi_enable(dsi);
	mtk_dsi_phy_timconfig(dsi, NULL);

	mtk_dsi_rxtx_control(dsi);
	mtk_dsi_ps_control_vact(dsi);
	if (!mtk_dsi_is_cmd_mode(&dsi->ddp_comp)) {
		mtk_dsi_set_vm_cmd(dsi);
		mtk_dsi_calc_vdo_timing(dsi);
		mtk_dsi_config_vdo_timing(dsi);
	}

	mtk_dsi_set_interrupt_enable(dsi);

	mtk_dsi_exit_ulps(dsi);
	mtk_dsi_clk_hs_mode(dsi, 0);

	if (dsi->panel) {
		if ((!dsi->doze_enabled || force_lcm_update)
			&& drm_panel_prepare(dsi->panel)) {
			DDPPR_ERR("failed to prepare the panel\n");
			return;
		}

		if (dsi->ddp_comp.mtk_crtc && dsi->ddp_comp.mtk_crtc->esd_ctx) {
			dsi->ddp_comp.mtk_crtc->esd_ctx->panel_init = true;
		}

		/* add for ESD recovery */
		if (mtk_dsi_is_cmd_mode(&dsi->ddp_comp) && mode_id != 0) {
			if (dsi->ext && dsi->ext->funcs &&
				dsi->ext->funcs->mode_switch) {
				DDPMSG("%s do lcm mode_switch to %u\n",
					__func__, mode_id);
				dsi->ext->funcs->mode_switch(dsi->panel, 0,
					mode_id, AFTER_DSI_POWERON);
			}
		}

		pr_info("%s new_doze_state = %d, dsi->doze_enabled = %d, dsi->esd_recovery_flag = %d\n",
			__func__, new_doze_state, dsi->doze_enabled, dsi->esd_recovery_flag);
		if (new_doze_state && !dsi->doze_enabled) {
			if (ext && ext->funcs &&
				ext->funcs->doze_enable_start)
				ext->funcs->doze_enable_start(dsi->panel, dsi,
					mipi_dsi_dcs_write_gce2, NULL);
			if (ext && ext->funcs
				&& ext->funcs->doze_enable)
				ext->funcs->doze_enable(dsi->panel, dsi,
					mipi_dsi_dcs_write_gce2, NULL);
			if (ext && ext->funcs
				&& ext->funcs->doze_area)
				ext->funcs->doze_area(dsi->panel, dsi,
					mipi_dsi_dcs_write_gce2, NULL);
		}
		/* for esd recovery in aod */
		if (new_doze_state && dsi->esd_recovery_flag) {
			if (ext && ext->funcs
				&& ext->funcs->doze_enable)
				ext->funcs->doze_enable(dsi->panel, dsi,
					mipi_dsi_dcs_write_gce2, NULL);
		}
		if (!new_doze_state && dsi->doze_enabled) {
			if (ext && ext->funcs
				&& ext->funcs->doze_disable)
				ext->funcs->doze_disable(dsi->panel, dsi,
					mipi_dsi_dcs_write_gce2, NULL);
		}
	}

	/*
	 * TODO: It's a temp workaround for cmd mode. When set the EXT_TE_EN bit
	 * before sending DSI cmd. System would hang. So move the bit control
	 * after
	 * lcm initialize.
	 */
	if (mtk_dsi_is_cmd_mode(&dsi->ddp_comp))
		writel(0x0001023c, dsi->regs + DSI_TXRX_CTRL);

	mtk_dsi_set_mode(dsi);
	mtk_dsi_clk_hs_mode(dsi, 1);

	if (!mtk_dsi_is_cmd_mode(&dsi->ddp_comp))
		mtk_dsi_start(dsi);

	if (dsi->panel) {
		if (drm_panel_enable(dsi->panel)) {
			DDPPR_ERR("failed to enable the panel\n");
			goto err_dsi_power_off;
		}
		mtk_dsi_crtc_notify(dsi);

		/* Suspend to Doze */
		if (mtk_dsi_doze_status_change(dsi)) {
			/* We use doze_get_mode_flags to determine if
			 * there has CV switch in Doze mode.
			 */
			if (ext && ext->funcs
				&& ext->funcs->doze_post_disp_on
				&& ext->funcs->doze_get_mode_flags)
				ext->funcs->doze_post_disp_on(dsi->panel,
					dsi, mipi_dsi_dcs_write_gce2, NULL);
		}
	}

	//drm_notifier_call_chain(DRM_EVENT_BLANK, &g_notify_data);
	DDPDSIINFO("%s -\n", __func__);

	dsi->output_en = true;
	dsi->doze_enabled = new_doze_state;

	return;
err_dsi_power_off:
	mtk_dsi_stop(dsi);
	mtk_dsi_poweroff(dsi);
}

static int mtk_dsi_stop_vdo_mode(struct mtk_dsi *dsi, void *handle);
static int mtk_dsi_wait_cmd_frame_done(struct mtk_dsi *dsi,
	int force_lcm_update)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(dsi->encoder.crtc);
	struct cmdq_pkt *handle;
	bool new_doze_state = mtk_dsi_doze_state(dsi);

	mtk_crtc_pkt_create(&handle,
		&mtk_crtc->base,
		mtk_crtc->gce_obj.client[CLIENT_CFG]);

	/* wait frame done */
	cmdq_pkt_wait_no_clear(handle,
		mtk_crtc->gce_obj.event[EVENT_STREAM_EOF]);

	/* When system ready to go to Doze suspend stage, it has to
	 * update the latest image before entering it to make sure display
	 * correctly. Since it's hard to know how many frame config GCE
	 * commands are there in the waiting queue, so here we force
	 * frame updating and wait for the latest frame done.
	 */
	if (new_doze_state && !force_lcm_update) {
		cmdq_pkt_set_event(handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_DIRTY]);
		cmdq_pkt_wait_no_clear(handle,
			mtk_crtc->gce_obj.event[EVENT_CMD_EOF]);
	}

	cmdq_pkt_clear_event(
		handle,
		mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);

	cmdq_pkt_flush(handle);
	cmdq_pkt_destroy(handle);
	return 0;
}

static void mtk_output_dsi_disable(struct mtk_dsi *dsi,
	int force_lcm_update)
{
	bool new_doze_state = mtk_dsi_doze_state(dsi);
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(dsi->encoder.crtc);

	DDPDSIINFO("%s+ doze_enabled:%d, dsi->doze_enabled = %d\n", __func__, new_doze_state, dsi->doze_enabled);
	if (dsi->doze_state == DRM_BLANK_UNBLANK) {
		dsi->doze_state = DRM_BLANK_POWERDOWN;
		pr_info("%s power down doze state\n", __func__);
	}
	//mkt_disp_esd_irq_ctrl(dsi->ddp_comp.mtk_crtc->esd_ctx, false);
	if (dsi->ddp_comp.mtk_crtc && dsi->ddp_comp.mtk_crtc->esd_ctx) {
		dsi->ddp_comp.mtk_crtc->esd_ctx->panel_init = false;
	}
	//g_notify_data.data = &dsi->doze_state;

	if (!dsi->output_en)
		return;

	//drm_notifier_call_chain(DRM_EARLY_EVENT_BLANK, &g_notify_data);
	mtk_drm_crtc_wait_blank(mtk_crtc);

	/* 1. If not doze mode, turn off backlight */
	if (dsi->panel && (!new_doze_state || force_lcm_update)) {
		if (drm_panel_disable(dsi->panel)) {
			DRM_ERROR("failed to disable the panel\n");
			return;
		}
		mtk_dsi_crtc_notify(dsi);
	}

	/* 2. If VDO mode, stop it and set to CMD mode */
	if (!mtk_dsi_is_cmd_mode(&dsi->ddp_comp))
		mtk_dsi_stop_vdo_mode(dsi, NULL);
	else
		mtk_dsi_wait_cmd_frame_done(dsi, force_lcm_update);

	/* 3. turn off panel or set to doze mode */
	if (dsi->panel) {
		if (!new_doze_state || force_lcm_update) {
			if (drm_panel_unprepare(dsi->panel))
				DRM_ERROR("failed to unprepare the panel\n");
		} else if (new_doze_state && !dsi->doze_enabled) {
			mtk_output_en_doze_switch(dsi);
		}
	}

	/* set DSI into ULPS mode */
	mtk_dsi_reset_engine(dsi);

	mtk_dsi_enter_ulps(dsi);

	mtk_dsi_disable(dsi);
	mtk_dsi_stop(dsi);

	mtk_dsi_poweroff(dsi);

	//drm_notifier_call_chain(DRM_EVENT_BLANK, &g_notify_data);

	dsi->output_en = false;
	dsi->doze_enabled = new_doze_state;
	DDPDSIINFO("%s-\n", __func__);
}

static void mtk_dsi_encoder_destroy(struct drm_encoder *encoder)
{
	drm_encoder_cleanup(encoder);
}

static const struct drm_encoder_funcs mtk_dsi_encoder_funcs = {
	.destroy = mtk_dsi_encoder_destroy,
};

static bool mtk_dsi_encoder_mode_fixup(struct drm_encoder *encoder,
				       const struct drm_display_mode *mode,
				       struct drm_display_mode *adjusted_mode)
{
	return true;
}

static void mtk_dsi_encoder_mode_set(struct drm_encoder *encoder,
				     struct drm_display_mode *mode,
				     struct drm_display_mode *adjusted)
{
	struct mtk_dsi *dsi = encoder_to_dsi(encoder);

	dsi->vm.pixelclock = adjusted->clock;
	dsi->vm.hactive = adjusted->hdisplay;
	dsi->vm.hback_porch = adjusted->htotal - adjusted->hsync_end;
	dsi->vm.hfront_porch = adjusted->hsync_start - adjusted->hdisplay;
	dsi->vm.hsync_len = adjusted->hsync_end - adjusted->hsync_start;

	dsi->vm.vactive = adjusted->vdisplay;
	dsi->vm.vback_porch = adjusted->vtotal - adjusted->vsync_end;
	dsi->vm.vfront_porch = adjusted->vsync_start - adjusted->vdisplay;
	dsi->vm.vsync_len = adjusted->vsync_end - adjusted->vsync_start;
}

static void mtk_dsi_encoder_disable(struct drm_encoder *encoder)
{
	struct mtk_dsi *dsi = encoder_to_dsi(encoder);
	struct drm_crtc *crtc = encoder->crtc;
	int index = drm_crtc_index(crtc);

	CRTC_MMP_EVENT_START(index, dsi_suspend,
			(unsigned long)crtc, index);

	DDPINFO("%s\n", __func__);
	mtk_drm_idlemgr_kick(__func__, crtc, 0);
	mtk_output_dsi_disable(dsi, false);

	CRTC_MMP_EVENT_END(index, dsi_suspend,
			(unsigned long)dsi->output_en, 0);
}

static void mtk_dsi_encoder_enable(struct drm_encoder *encoder)
{
	struct mtk_dsi *dsi = encoder_to_dsi(encoder);
	struct drm_crtc *crtc = encoder->crtc;
	int index = drm_crtc_index(crtc);

	CRTC_MMP_EVENT_START(index, dsi_resume,
			(unsigned long)crtc, index);

	DDPINFO("%s\n", __func__);
	mtk_output_dsi_enable(dsi, false);

	CRTC_MMP_EVENT_END(index, dsi_resume,
			(unsigned long)dsi->output_en, 0);
}

static enum drm_connector_status
mtk_dsi_connector_detect(struct drm_connector *connector, bool force)
{
	return connector_status_connected;
}

static int mtk_dsi_connector_get_modes(struct drm_connector *connector)
{
	struct mtk_dsi *dsi = connector_to_dsi(connector);

	return drm_panel_get_modes(dsi->panel);
}

static int mtk_dsi_atomic_check(struct drm_encoder *encoder,
				struct drm_crtc_state *crtc_state,
				struct drm_connector_state *conn_state)
{
	struct mtk_drm_crtc *mtk_crtc =
		container_of(conn_state->crtc, struct mtk_drm_crtc, base);
	struct mtk_dsi *dsi = encoder_to_dsi(encoder);

	switch (dsi->format) {
	case MIPI_DSI_FMT_RGB565:
		mtk_crtc->bpc = 5;
		break;
	case MIPI_DSI_FMT_RGB666_PACKED:
		mtk_crtc->bpc = 6;
		break;
	case MIPI_DSI_FMT_RGB666:
	case MIPI_DSI_FMT_RGB888:
	default:
		mtk_crtc->bpc = 8;
		break;
	}

	return 0;
}

static const struct drm_encoder_helper_funcs mtk_dsi_encoder_helper_funcs = {
	.mode_fixup = mtk_dsi_encoder_mode_fixup,
	.mode_set = mtk_dsi_encoder_mode_set,
	.disable = mtk_dsi_encoder_disable,
	.enable = mtk_dsi_encoder_enable,
	.atomic_check = mtk_dsi_atomic_check,
};

static const struct drm_connector_funcs mtk_dsi_connector_funcs = {
	/* .dpms = drm_atomic_helper_connector_dpms, */
	.detect = mtk_dsi_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static const struct drm_connector_helper_funcs mtk_dsi_conn_helper_funcs = {
	.get_modes = mtk_dsi_connector_get_modes,
};

static int mtk_drm_attach_bridge(struct drm_bridge *bridge,
				 struct drm_encoder *encoder)
{
	int ret;

	if (!bridge)
		return -ENOENT;

	encoder->bridge = bridge;
	bridge->encoder = encoder;
	ret = drm_bridge_attach(encoder, bridge, NULL);
	if (ret) {
		DRM_ERROR("Failed to attach bridge to drm\n");
		encoder->bridge = NULL;
		bridge->encoder = NULL;
	}

	return ret;
}

static int mtk_dsi_create_connector(struct drm_device *drm, struct mtk_dsi *dsi)
{
	int ret;

	ret = drm_connector_init(drm, &dsi->conn, &mtk_dsi_connector_funcs,
				 DRM_MODE_CONNECTOR_DSI);
	if (ret) {
		DRM_ERROR("Failed to connector init to drm\n");
		return ret;
	}

	drm_connector_helper_add(&dsi->conn, &mtk_dsi_conn_helper_funcs);

	dsi->conn.dpms = DRM_MODE_DPMS_OFF;
	drm_mode_connector_attach_encoder(&dsi->conn, &dsi->encoder);

	if (dsi->panel) {
		ret = drm_panel_attach(dsi->panel, &dsi->conn);
		if (ret) {
			DRM_ERROR("Failed to attach panel to drm\n");
			goto err_connector_cleanup;
		}
	}

	return 0;

err_connector_cleanup:
	drm_connector_cleanup(&dsi->conn);
	return ret;
}

static int mtk_dsi_create_conn_enc(struct drm_device *drm, struct mtk_dsi *dsi)
{
	int ret;

	ret = drm_encoder_init(drm, &dsi->encoder, &mtk_dsi_encoder_funcs,
			       DRM_MODE_ENCODER_DSI, NULL);
	if (ret) {
		DRM_ERROR("Failed to encoder init to drm\n");
		return ret;
	}
	drm_encoder_helper_add(&dsi->encoder, &mtk_dsi_encoder_helper_funcs);

	/*
	 * Currently display data paths are statically assigned to a crtc each.
	 * crtc 0 is OVL0 -> COLOR0 -> AAL -> OD -> RDMA0 -> UFOE -> DSI0
	 */
	dsi->encoder.possible_crtcs = 1;

	/* If there's a bridge, attach to it and let it create the connector */
	ret = mtk_drm_attach_bridge(dsi->bridge, &dsi->encoder);
	if (ret) {
		/* Otherwise create our own connector and attach to a panel */
		ret = mtk_dsi_create_connector(drm, dsi);
		if (ret)
			goto err_encoder_cleanup;
	}

	return 0;

err_encoder_cleanup:
	drm_encoder_cleanup(&dsi->encoder);
	return ret;
}

static void mtk_dsi_destroy_conn_enc(struct mtk_dsi *dsi)
{
	drm_encoder_cleanup(&dsi->encoder);
	/* Skip connector cleanup if creation was delegated to the bridge */
	if (dsi->conn.dev)
		drm_connector_cleanup(&dsi->conn);
}

struct mtk_panel_ext *mtk_dsi_get_panel_ext(struct mtk_ddp_comp *comp)
{
	struct mtk_dsi *dsi = container_of(comp, struct mtk_dsi, ddp_comp);

	return dsi->ext;
}

/* SET MODE */
static void _mtk_dsi_set_mode(struct mtk_ddp_comp *comp, void *handle,
			      unsigned int mode)
{
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DSI_MODE_CTRL,
		       mode, ~0);
}

/* STOP VDO MODE */
static int mtk_dsi_stop_vdo_mode(struct mtk_dsi *dsi, void *handle)
{
	struct mtk_ddp_comp *comp = &dsi->ddp_comp;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	int need_create_hnd = 0;
	struct cmdq_pkt *cmdq_handle;

	if (!mtk_crtc) {
		DDPPR_ERR("%s, mtk_crtc is NULL\n", __func__);
		return 1;
	}

	/* Add blocking flush for waiting dsi idle in other gce client */
	if (handle) {
		struct cmdq_pkt *cmdq_handle1 = (struct cmdq_pkt *)handle;

		if (cmdq_handle1->cl !=
				mtk_crtc->gce_obj.client[CLIENT_DSI_CFG]) {
			mtk_crtc_pkt_create(&cmdq_handle,
				&mtk_crtc->base,
				mtk_crtc->gce_obj.client[CLIENT_DSI_CFG]);
			cmdq_pkt_flush(cmdq_handle);
			cmdq_pkt_destroy(cmdq_handle);
		}
	} else {
		mtk_crtc_pkt_create(&cmdq_handle,
			&mtk_crtc->base,
			mtk_crtc->gce_obj.client[CLIENT_DSI_CFG]);
		cmdq_pkt_flush(cmdq_handle);
		cmdq_pkt_destroy(cmdq_handle);
	}

	if (!handle)
		need_create_hnd = 1;
	if (need_create_hnd) {
		mtk_crtc_pkt_create((struct cmdq_pkt **)&handle,
			&mtk_crtc->base,
			mtk_crtc->gce_obj.client[CLIENT_CFG]);

		/* wait frame done */
		cmdq_pkt_wait_no_clear(handle,
		   mtk_crtc->gce_obj.event[EVENT_VDO_EOF]);
	}
	/* stop vdo mode */
	_mtk_dsi_set_mode(&dsi->ddp_comp, handle, CMD_MODE);
	cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
		dsi->ddp_comp.regs_pa + DSI_START, 0, ~0);
	mtk_dsi_poll_for_idle(dsi, handle);

	if (need_create_hnd) {
		cmdq_pkt_flush(handle);
		cmdq_pkt_destroy(handle);
	}
	return 0;
}

static int mtk_dsi_start_vdo_mode(struct mtk_ddp_comp *comp, void *handle)
{
	struct mtk_dsi *dsi = container_of(comp, struct mtk_dsi, ddp_comp);
	u32 vid_mode = CMD_MODE;

	if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO) {
		if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_BURST)
			vid_mode = BURST_MODE;
		else if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_SYNC_PULSE)
			vid_mode = SYNC_PULSE_MODE;
		else
			vid_mode = SYNC_EVENT_MODE;
	}

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DSI_START, 0,
		       ~0);

	_mtk_dsi_set_mode(comp, handle, vid_mode);

	return 0;
}

static int mtk_dsi_trigger(struct mtk_ddp_comp *comp, void *handle)
{
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DSI_START, 1,
		       ~0);

	return 0;
}

int mtk_dsi_read_gce(struct mtk_ddp_comp *comp, void *handle,
			struct DSI_T0_INS *t0, int i, uintptr_t slot)
{
	struct mtk_dsi *dsi = container_of(comp, struct mtk_dsi, ddp_comp);
	dma_addr_t read_slot = (dma_addr_t)slot;

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DSI_CMDQ0,
		0x00013700, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DSI_CMDQ1,
		AS_UINT32(t0), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DSI_CMDQ_SIZE,
		0x2, ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DSI_START,
		0x0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DSI_START,
		0x1, ~0);

	mtk_dsi_cmdq_poll(comp, handle, comp->regs_pa + DSI_INTSTA, 0x1, 0x1);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DSI_INTSTA,
		0x0, 0x1);

	cmdq_pkt_mem_move(handle, comp->cmdq_base,
		comp->regs_pa + DSI_RX_DATA0, read_slot + (i * 2) * 0x4,
		CMDQ_THR_SPR_IDX3);
	cmdq_pkt_mem_move(handle, comp->cmdq_base,
		comp->regs_pa + DSI_RX_DATA1, read_slot + (i * 2 + 1) * 0x4,
		CMDQ_THR_SPR_IDX3);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DSI_RACK,
		0x1, 0x1);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DSI_INTSTA,
		0x0, 0x1);

	mtk_dsi_poll_for_idle(dsi, handle);

	return 0;
}

int mtk_dsi_esd_read(struct mtk_ddp_comp *comp, void *handle, uintptr_t slot)
{
	int i;
	struct DSI_T0_INS t0;
	struct mtk_dsi *dsi = container_of(comp, struct mtk_dsi, ddp_comp);
	struct mtk_panel_params *params;

	if (dsi->ext && dsi->ext->params)
		params = dsi->ext->params;
	else /* can't find panel ext information, stop esd read */
		return 0;

	for (i = 0 ; i < ESD_CHECK_NUM ; i++) {

		if (params->lcm_esd_check_table[i].cmd == 0)
			break;

		t0.CONFG = 0x04;
		t0.Data0 = params->lcm_esd_check_table[i].cmd;
		t0.Data_ID = (t0.Data0 < 0xB0)
				     ? DSI_DCS_READ_PACKET_ID
				     : DSI_GERNERIC_READ_LONG_PACKET_ID;
		t0.Data1 = 0;

		mtk_dsi_read_gce(comp, handle, &t0, i, slot);
	}

	return 0;
}

int mtk_dsi_esd_cmp(struct mtk_ddp_comp *comp, void *handle, void *slot)
{
	int i, ret = 0;
	u32 tmp0, tmp1, chk_val;
	struct mtk_dsi *dsi = container_of(comp, struct mtk_dsi, ddp_comp);
	struct esd_check_item *lcm_esd_tb;
	struct mtk_panel_params *params;

	if (dsi->ext && dsi->ext->params)
		params = dsi->ext->params;
	else /* can't find panel ext information, stop esd read */
		return 0;

	for (i = 0; i < ESD_CHECK_NUM; i++) {
		if (dsi->ext->params->lcm_esd_check_table[i].cmd == 0)
			break;

		if (slot) {
			tmp0 = AS_UINT32(slot + (i * 2) * 0x4);
			tmp1 = AS_UINT32(slot + (i * 2 + 1) * 0x4);
		} else if (i == 0) {
			tmp0 = readl(dsi->regs + DSI_RX_DATA0);
			tmp1 = readl(dsi->regs + DSI_RX_DATA1);
		}

		lcm_esd_tb = &params->lcm_esd_check_table[i];

		if ((tmp0 & 0xff) == 0x1C)
			chk_val = tmp1 & 0xff;
		else
			chk_val = (tmp0 >> 8) & 0xff;

		if (lcm_esd_tb->mask_list[0])
			chk_val = chk_val & lcm_esd_tb->mask_list[0];

		if (chk_val == lcm_esd_tb->para_list[0]) {
			ret = 0;
		} else {
			DDPPR_ERR("[DSI]cmp fail:read(0x%x)!=expect(0x%x)\n",
				  chk_val, lcm_esd_tb->para_list[0]);
			ret = -1;
			break;
		}
	}

	return ret;
}

static const char *mtk_dsi_cmd_mode_parse_state(unsigned int state)
{
	switch (state) {
	case 0x0001:
		return "idle";
	case 0x0002:
		return "Reading command queue for header";
	case 0x0004:
		return "Sending type-0 command";
	case 0x0008:
		return "Waiting frame data from RDMA for type-1 command";
	case 0x0010:
		return "Sending type-1 command";
	case 0x0020:
		return "Sending type-2 command";
	case 0x0040:
		return "Reading command queue for type-2 data";
	case 0x0080:
		return "Sending type-3 command";
	case 0x0100:
		return "Sending BTA";
	case 0x0200:
		return "Waiting RX-read data";
	case 0x0400:
		return "Waiting SW RACK for RX-read data";
	case 0x0800:
		return "Waiting TE";
	case 0x1000:
		return "Get TE";
	case 0x2000:
		return "Waiting SW RACK for TE";
	case 0x4000:
		return "Waiting external TE";
	case 0x8000:
		return "Get external TE";
	default:
		return "unknown";
	}
}

static const char *mtk_dsi_vdo_mode_parse_state(unsigned int state)
{
	switch (state) {
	case 0x0001:
		return "Video mode idle";
	case 0x0002:
		return "Sync start packet";
	case 0x0004:
		return "Hsync active";
	case 0x0008:
		return "Sync end packet";
	case 0x0010:
		return "Hsync back porch";
	case 0x0020:
		return "Video data period";
	case 0x0040:
		return "Hsync front porch";
	case 0x0080:
		return "BLLP";
	case 0x0100:
		return "--";
	case 0x0200:
		return "Mix mode using command mode transmission";
	case 0x0400:
		return "Command transmission in BLLP";
	default:
		return "unknown";
	}
}

int mtk_dsi_dump(struct mtk_ddp_comp *comp)
{
	int k;
	struct mtk_dsi *dsi = container_of(comp, struct mtk_dsi, ddp_comp);
	void __iomem *baddr = comp->regs;
	unsigned int reg_val;

	if (DISP_REG_GET_FIELD(MODE_FLD_REG_MODE_CON,
				   baddr + DSI_MODE_CTRL)) {
		/* VDO mode */
		reg_val = (readl(dsi->regs + 0x164)) & 0xff;
		DDPDUMP("state7(vdo mode):%s\n",
			mtk_dsi_vdo_mode_parse_state(reg_val));
	} else {
		reg_val = (readl(dsi->regs + 0x160)) & 0xffff;
		DDPDUMP("state6(cmd mode):%s\n",
			mtk_dsi_cmd_mode_parse_state(reg_val));
	}
	reg_val = (readl(dsi->regs + 0x168)) & 0x3fff;
	DDPDUMP("state8 WORD_COUNTER(cmd mode):%u\n", reg_val);
	reg_val = (readl(dsi->regs + 0x16C)) & 0x3fffff;
	DDPDUMP("state9 LINE_COUNTER(cmd mode):%u\n", reg_val);

	DDPDUMP("== %s REGS ==\n", mtk_dump_comp_str(comp));
	for (k = 0; k < 0x200; k += 16) {
		DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
			readl(dsi->regs + k),
			readl(dsi->regs + k + 0x4),
			readl(dsi->regs + k + 0x8),
			readl(dsi->regs + k + 0xc));
	}

	DDPDUMP("- DSI CMD REGS -\n");
	for (k = 0; k < 32; k += 16) {
		DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
			readl(dsi->regs + 0x200 + k),
			readl(dsi->regs + 0x200 + k + 0x4),
			readl(dsi->regs + 0x200 + k + 0x8),
			readl(dsi->regs + 0x200 + k + 0xc));
	}

	mtk_mipi_tx_dump(dsi->phy);

	return 0;
}

unsigned int mtk_dsi_fps_change_index(struct mtk_dsi *dsi,
	struct mtk_drm_crtc *mtk_crtc, struct drm_crtc_state *old_state)
{
	struct mtk_panel_ext *panel_ext = mtk_crtc->panel_ext;
	struct mtk_panel_ext *get_panel_ext = find_panel_ext(dsi->panel);
	struct drm_display_mode *old_mode;
	struct drm_display_mode *adjust_mode;
	struct mtk_panel_params *cur_panel_params = panel_ext->params;
	struct mtk_panel_params *adjust_panel_params = NULL;
	unsigned int fps_chg_index = 0;
	unsigned int old_get_sta = 0, new_get_sta = 0;

	struct mtk_crtc_state *state =
	    to_mtk_crtc_state(mtk_crtc->base.state);
	struct mtk_crtc_state *old_mtk_state =
	    to_mtk_crtc_state(old_state);
	unsigned int src_mode_idx =
	    old_mtk_state->prop_val[CRTC_PROP_DISP_MODE_IDX];
	unsigned int dst_mode_idx =
	    state->prop_val[CRTC_PROP_DISP_MODE_IDX];

	old_mode = &(mtk_crtc->avail_modes[src_mode_idx]);
	adjust_mode = &(mtk_crtc->avail_modes[dst_mode_idx]);

	if (panel_ext && panel_ext->funcs &&
		panel_ext->funcs->ext_param_set) {
		DDPINFO("old ext_param_set\n");
		old_get_sta = panel_ext->funcs->ext_param_set(
			dsi->panel, src_mode_idx);
	}
	if (old_get_sta)
		DDPINFO("%s,error:not support src MODE:(%d)\n", __func__,
			src_mode_idx);

	if (get_panel_ext) {
		cur_panel_params = get_panel_ext->params;
		adjust_panel_params = get_panel_ext->params;
	}

	if (panel_ext && panel_ext->funcs &&
		panel_ext->funcs->ext_param_set) {
		DDPINFO("new ext_param_set\n");
		new_get_sta = panel_ext->funcs->ext_param_set(
			dsi->panel, dst_mode_idx);
	}
	if (new_get_sta)
		DDPINFO("%s,error:not support dst MODE:(%d)\n", __func__,
			dst_mode_idx);

	if (!(dsi->mipi_hopping_sta && adjust_panel_params &&
		cur_panel_params && cur_panel_params->dyn.switch_en &&
		adjust_panel_params->dyn.switch_en == 1)) {
		if (adjust_mode->vtotal !=
			old_mode->vtotal) {
			fps_chg_index |= DYNFPS_DSI_VFP;
		}
		if (adjust_mode->htotal !=
			old_mode->htotal) {
			fps_chg_index |= DYNFPS_DSI_HFP;
		}
		if (panel_ext && adjust_panel_params &&
			panel_ext->params->data_rate !=
			adjust_panel_params->data_rate) {
			fps_chg_index |= DYNFPS_DSI_MIPI_CLK;
		}
		if (!fps_chg_index && cur_panel_params &&
			adjust_panel_params && cur_panel_params->pll_clk !=
			adjust_panel_params->pll_clk) {
			fps_chg_index |= DYNFPS_DSI_MIPI_CLK;
		}
		if (!fps_chg_index &&
			adjust_mode->clock != old_mode->clock) {
			fps_chg_index |= DYNFPS_DSI_MIPI_CLK;
		}
	} else if (cur_panel_params && adjust_panel_params) {
		if (cur_panel_params->dyn.vfp !=
			adjust_panel_params->dyn.vfp) {
			fps_chg_index |= DYNFPS_DSI_VFP;
		}
		if (cur_panel_params->dyn.hfp !=
			adjust_panel_params->dyn.hfp) {
			fps_chg_index |= DYNFPS_DSI_HFP;
		}
		if (cur_panel_params->dyn.pll_clk !=
			adjust_panel_params->dyn.pll_clk) {
			fps_chg_index |= DYNFPS_DSI_MIPI_CLK;
		}
		if (cur_panel_params->dyn.data_rate !=
			adjust_panel_params->dyn.data_rate) {
			fps_chg_index |= DYNFPS_DSI_MIPI_CLK;
		}
	}

	mtk_crtc->fps_change_index = fps_chg_index;
	DDPINFO("%s,chg %d->%d\n", __func__, old_mode->vrefresh,
		adjust_mode->vrefresh);
	DDPINFO("%s,mipi_hopping_sta %d,chg solution:0x%x\n", __func__,
		dsi->mipi_hopping_sta, fps_chg_index);
	return 0;
}

static const char *mtk_dsi_mode_spy(enum DSI_MODE_CON mode)
{
	switch (mode) {
	case MODE_CON_CMD:
		return "CMD_MODE";
	case MODE_CON_SYNC_PULSE_VDO:
		return "SYNC_PULSE_VDO_MODE";
	case MODE_CON_SYNC_EVENT_VDO:
		return "SYNC_EVENT_VDO_MODE";
	case MODE_CON_BURST_VDO:
		return "BURST_VDO_MODE";
	default:
		break;
	}
	return "unknown-mode";
}

int mtk_dsi_analysis(struct mtk_ddp_comp *comp)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_dsi *dsi = container_of(comp, struct mtk_dsi, ddp_comp);
#endif
	void __iomem *baddr = comp->regs;
	unsigned int reg_val;

	DDPDUMP("== %s ANALYSIS ==\n", mtk_dump_comp_str(comp));
#ifndef CONFIG_FPGA_EARLY_PORTING
	DDPDUMP("MIPITX Clock:%d\n", mtk_mipi_tx_pll_get_rate(dsi->phy));
#endif

	DDPDUMP("start:%x,busy:%d,DSI_DUAL_EN:%d\n",
		DISP_REG_GET_FIELD(START_FLD_REG_START, baddr + DSI_START),
		DISP_REG_GET_FIELD(INTSTA_FLD_REG_BUSY, baddr + DSI_INTSTA),
		DISP_REG_GET_FIELD(CON_CTRL_FLD_REG_DUAL_EN,
				   baddr + DSI_CON_CTRL));
	DDPDUMP("mode:%s,high_speed:%d,FSM_State:%s\n",
		mtk_dsi_mode_spy(DISP_REG_GET_FIELD(MODE_FLD_REG_MODE_CON,
						    baddr + DSI_MODE_CTRL)),
		DISP_REG_GET_FIELD(PHY_FLD_REG_LC_HSTX_EN,
				   baddr + DSI_PHY_LCCON),
		mtk_dsi_cmd_mode_parse_state(
			DISP_REG_GET_FIELD(STATE_DBG6_FLD_REG_CMCTL_STATE,
					   baddr + DSI_STATE_DBG6)));

	reg_val = readl(DSI_INTEN + baddr);
	DDPDUMP("IRQ_EN,RD_RDY:%d,CMD_DONE:%d,SLEEPOUT_DONE:%d\n",
		REG_FLD_VAL_GET(INTSTA_FLD_REG_RD_RDY, reg_val),
		REG_FLD_VAL_GET(INTSTA_FLD_REG_CMD_DONE, reg_val),
		REG_FLD_VAL_GET(INTSTA_FLD_REG_SLEEPOUT_DONE, reg_val));
	DDPDUMP("TE_RDY:%d,VM_CMD_DONE:%d,VM_DONE:%d\n",
		REG_FLD_VAL_GET(INTSTA_FLD_REG_TE_RDY, reg_val),
		REG_FLD_VAL_GET(INTSTA_FLD_REG_VM_CMD_DONE, reg_val),
		REG_FLD_VAL_GET(INTSTA_FLD_REG_VM_DONE, reg_val));

	reg_val = readl(DSI_INTSTA + baddr);
	DDPDUMP("IRQ,RD_RDY:%d,CMD_DONE:%d,SLEEPOUT_DONE:%d\n",
		REG_FLD_VAL_GET(INTSTA_FLD_REG_RD_RDY, reg_val),
		REG_FLD_VAL_GET(INTSTA_FLD_REG_CMD_DONE, reg_val),
		REG_FLD_VAL_GET(INTSTA_FLD_REG_SLEEPOUT_DONE, reg_val));
	DDPDUMP("TE_RDY:%d,VM_CMD_DONE:%d,VM_DONE:%d\n",
		REG_FLD_VAL_GET(INTSTA_FLD_REG_TE_RDY, reg_val),
		REG_FLD_VAL_GET(INTSTA_FLD_REG_VM_CMD_DONE, reg_val),
		REG_FLD_VAL_GET(INTSTA_FLD_REG_VM_DONE, reg_val));

	reg_val = readl(DSI_TXRX_CTRL + baddr);
	DDPDUMP("lane_num:%d,Ext_TE_EN:%d,Ext_TE_Edge:%d,HSTX_CKLP_EN:%d\n",
		REG_FLD_VAL_GET(TXRX_CTRL_FLD_REG_LANE_NUM, reg_val),
		REG_FLD_VAL_GET(TXRX_CTRL_FLD_REG_EXT_TE_EN, reg_val),
		REG_FLD_VAL_GET(TXRX_CTRL_FLD_REG_EXT_TE_EDGE, reg_val),
		REG_FLD_VAL_GET(TXRX_CTRL_FLD_REG_HSTX_CKLP_EN, reg_val));

	reg_val = readl(DSI_LFR_CON + baddr);
	DDPDUMP("LFR_en:%d, LFR_VSE_DIS:%d, LFR_UPDATE:%d, LFR_MODE:%d, LFR_TYPE:%d, LFR_SKIP_NUMBER:%d\n",
		REG_FLD_VAL_GET(LFR_CON_FLD_REG_LFR_EN, reg_val),
		REG_FLD_VAL_GET(LFR_CON_FLD_REG_LFR_VSE_DIS, reg_val),
		REG_FLD_VAL_GET(LFR_CON_FLD_REG_LFR_UPDATE, reg_val),
		REG_FLD_VAL_GET(LFR_CON_FLD_REG_LFR_MODE, reg_val),
		REG_FLD_VAL_GET(LFR_CON_FLD_REG_LFR_TYPE, reg_val),
		REG_FLD_VAL_GET(LFR_CON_FLD_REG_LFR_SKIP_NUM, reg_val));

	return 0;
}

static void mtk_dsi_ddp_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_dsi *dsi = container_of(comp, struct mtk_dsi, ddp_comp);

	mtk_dsi_poweron(dsi);
}

static void mtk_dsi_ddp_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_dsi *dsi = container_of(comp, struct mtk_dsi, ddp_comp);

	mtk_dsi_poweroff(dsi);
}

static void mtk_dsi_config_trigger(struct mtk_ddp_comp *comp,
				   struct cmdq_pkt *handle,
				   enum mtk_ddp_comp_trigger_flag flag)
{
	struct mtk_dsi *dsi = container_of(comp, struct mtk_dsi, ddp_comp);

	switch (flag) {
	case MTK_TRIG_FLAG_TRIGGER:
		/* TODO: avoid hardcode: 0xF0 register offset  */
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->mtk_crtc->config_regs_pa + 0xF0, 0x1, 0x1);

		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + 0x200,
			       0x002c3909, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + 0x60, 1,
			       ~0);

		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DSI_START, 0, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DSI_START, 1, ~0);
		break;
	case MTK_TRIG_FLAG_EOF:
		mtk_dsi_poll_for_idle(dsi, handle);
		break;
	default:
		break;
	}
}

static int mtk_dsi_is_busy(struct mtk_ddp_comp *comp)
{
	int ret, tmp;
	struct mtk_dsi *dsi = container_of(comp, struct mtk_dsi, ddp_comp);

	tmp = readl(dsi->regs + DSI_INTSTA);
	ret = (tmp & DSI_BUSY) ? 1 : 0;

	DDPINFO("%s:%d is:%d regs:0x%x\n", __func__, __LINE__, ret, tmp);

	return ret;
}

bool mtk_dsi_is_cmd_mode(struct mtk_ddp_comp *comp)
{
	struct mtk_dsi *dsi = NULL;

	if (mtk_ddp_comp_get_type(comp->id) == MTK_DISP_WDMA)
		return true;

	dsi = container_of(comp, struct mtk_dsi, ddp_comp);

	if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO)
		return false;
	else
		return true;
}
void mtk_dsi_esd_recovery_flag(struct mtk_ddp_comp *comp, struct drm_crtc *crtc)
{
	struct mtk_dsi *dsi;
	struct mtk_crtc_state *state;

	dsi = container_of(comp, struct mtk_dsi, ddp_comp);
	state = to_mtk_crtc_state(crtc->state);

	if (state->prop_val[CRTC_PROP_DOZE_ACTIVE] == 1) {
		dsi->esd_recovery_flag = true;
		pr_info("%s\n", __func__);
	}
}

int mtk_clear_esd_status(struct drm_connector *c)
{
	struct mtk_dsi *dsi = container_of(c, struct mtk_dsi, conn);
	if (!dsi) {
		DDPPR_ERR("%s dsi is null\n", __func__);
		return -1;
	}

	pr_info("%s\n", __func__);
	dsi->esd_recovery_flag = false;
	return 0;
}

bool mtk_get_esd_status(struct drm_connector *c)
{
	struct mtk_dsi *dsi = container_of(c, struct mtk_dsi, conn);
	if (!dsi) {
		DDPPR_ERR("%s dsi is null\n", __func__);
		return -1;
	}

	return dsi->esd_recovery_flag;
}

static const char *mtk_dsi_get_porch_str(enum dsi_porch_type type)
{
	if (type < 0) {
		DDPPR_ERR("%s: Invalid dsi porch type:%d\n", __func__, type);
		type = 0;
	}
	return mtk_dsi_porch_str[type];
}

int mtk_dsi_porch_setting(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum dsi_porch_type type, unsigned int value)
{
	int ret = 0;

	DDPINFO("%s set %s: %s to %d\n", __func__, mtk_dump_comp_str(comp),
		mtk_dsi_get_porch_str(type), value);

	switch (type) {
	case DSI_VFP:
		mtk_ddp_write_relaxed(comp, value, DSI_VFP_NL, handle);
		break;
	case DSI_VSA:
		mtk_ddp_write_relaxed(comp, value, DSI_VSA_NL, handle);
		break;
	case DSI_VBP:
		mtk_ddp_write_relaxed(comp, value, DSI_VBP_NL, handle);
		break;
	case DSI_VACT:
		mtk_ddp_write_relaxed(comp, value, DSI_VACT_NL, handle);
		break;
	case DSI_HFP:
		mtk_ddp_write_relaxed(comp, value, DSI_HFP_WC, handle);
		break;
	case DSI_HSA:
		mtk_ddp_write_relaxed(comp, value, DSI_HSA_WC, handle);
		break;
	case DSI_HBP:
		mtk_ddp_write_relaxed(comp, value, DSI_HBP_WC, handle);
		break;
	case DSI_BLLP:
		mtk_ddp_write_relaxed(comp, value, DSI_BLLP_WC, handle);
		break;
	default:
		break;
	}

	return ret;
}

/* TODO: refactor to remove duplicate code */
static void mtk_dsi_enter_idle(struct mtk_dsi *dsi)
{
	mtk_dsi_mask(dsi, DSI_INTEN, ~0, 0);

	mtk_dsi_reset_engine(dsi);

	mtk_dsi_enter_ulps(dsi);

	mtk_dsi_poweroff(dsi);
}

static void mtk_dsi_leave_idle(struct mtk_dsi *dsi)
{
	int ret;

	ret = mtk_dsi_poweron(dsi);

	if (ret < 0) {
		DDPPR_ERR("failed to power on dsi\n");
		return;
	}

	mtk_dsi_enable(dsi);
	mtk_dsi_phy_timconfig(dsi, NULL);

	mtk_dsi_rxtx_control(dsi);
	mtk_dsi_ps_control_vact(dsi);
	mtk_dsi_set_interrupt_enable(dsi);

	mtk_dsi_exit_ulps(dsi);

	/*
	 * TODO: It's a temp workaround for cmd mode. When set the EXT_TE_EN bit
	 * before sending DSI cmd. System would hang. So move the bit control
	 * after
	 * lcm initialize.
	 */
	if (mtk_dsi_is_cmd_mode(&dsi->ddp_comp))
		writel(0x0001023c, dsi->regs + DSI_TXRX_CTRL);

	mtk_dsi_set_mode(dsi);
	mtk_dsi_clk_hs_mode(dsi, 1);
}

static void mtk_dsi_clk_change(struct mtk_dsi *dsi, int en)
{
	struct mtk_panel_ext *ext = dsi->ext;
	struct mtk_ddp_comp *comp = &dsi->ddp_comp;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	bool mod_vfp, mod_vbp, mod_vsa;
	bool mod_hfp, mod_hbp, mod_hsa;
	unsigned int data_rate;
	struct cmdq_pkt *cmdq_handle;
	int index = 0;

	if (!crtc) {
		DDPPR_ERR("%s, crtc is NULL\n", __func__);
		return;
	}

	index = drm_crtc_index(crtc);

	dsi->mipi_hopping_sta = en;

	if (!(ext && ext->params &&
			ext->params->dyn.switch_en == 1))
		return;

	CRTC_MMP_EVENT_START(index, clk_change,
			en, (ext->params->data_rate << 16)
			| ext->params->pll_clk);

	mod_vfp = !!ext->params->dyn.vfp;
	mod_vbp = !!ext->params->dyn.vbp;
	mod_vsa = !!ext->params->dyn.vsa;
	mod_hfp = !!ext->params->dyn.hfp;
	mod_hbp = !!ext->params->dyn.hbp;
	mod_hsa = !!ext->params->dyn.hsa;

	if (en) {
		data_rate = !!ext->params->dyn.data_rate ?
				ext->params->dyn.data_rate :
				ext->params->dyn.pll_clk * 2;
	} else {
		data_rate = mtk_dsi_default_rate(dsi);
	}

	dsi->data_rate = data_rate;
	mtk_mipi_tx_pll_rate_set_adpt(dsi->phy, data_rate);

	/* implicit way for display power state */
	if (dsi->clk_refcnt == 0) {
		CRTC_MMP_MARK(index, clk_change, 0, 1);
		goto done;
	}

	if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO)
		mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
				mtk_crtc->gce_obj.client[CLIENT_DSI_CFG]);
	else
		mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
			mtk_crtc->gce_obj.client[CLIENT_CFG]);

	if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO) {
		mtk_dsi_calc_vdo_timing(dsi);

		cmdq_pkt_wait_no_clear(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_VDO_EOF]);

		mtk_dsi_phy_timconfig(dsi, cmdq_handle);

		if (mod_hfp)
			mtk_dsi_porch_setting(comp, cmdq_handle, DSI_HFP,
				dsi->hfp_byte);

		if (mod_hbp)
			mtk_dsi_porch_setting(comp, cmdq_handle, DSI_HBP,
				dsi->hbp_byte);

		if (mod_hsa)
			mtk_dsi_porch_setting(comp, cmdq_handle, DSI_HSA,
				dsi->hsa_byte);

		if (mod_vbp)
			mtk_dsi_porch_setting(comp, cmdq_handle,
				DSI_VBP, dsi->vbp);

		if (mod_vsa)
			mtk_dsi_porch_setting(comp, cmdq_handle,
				DSI_VSA, dsi->vsa);
	}

	mtk_mipi_tx_pll_rate_switch_gce(dsi->phy, cmdq_handle, data_rate);

	if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO) {
		cmdq_pkt_clear_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_DSI0_SOF]);
		cmdq_pkt_wait_no_clear(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_DSI0_SOF]);

		if (mod_vfp)
			mtk_dsi_porch_setting(comp, cmdq_handle,
				DSI_VFP, dsi->vfp);
	}

	cmdq_pkt_flush(cmdq_handle);
	cmdq_pkt_destroy(cmdq_handle);

done:
	CRTC_MMP_EVENT_END(index, clk_change,
			dsi->mode_flags,
			(ext->params->dyn.data_rate << 16) |
			ext->params->dyn.pll_clk);
}

static int mtk_dsi_host_attach(struct mipi_dsi_host *host,
			       struct mipi_dsi_device *device)
{
	struct mtk_dsi *dsi = host_to_dsi(host);

	dsi->lanes = device->lanes;
	dsi->format = device->format;
	dsi->mode_flags = device->mode_flags;
	/* ********Panel Master********** */
	dsi->dev_for_PM = device;
	/* ******end Panel Master**** */
	if (dsi->conn.dev)
		drm_helper_hpd_irq_event(dsi->conn.dev);

	return 0;
}

static int mtk_dsi_host_detach(struct mipi_dsi_host *host,
			       struct mipi_dsi_device *device)
{
	struct mtk_dsi *dsi = host_to_dsi(host);

	if (dsi->conn.dev)
		drm_helper_hpd_irq_event(dsi->conn.dev);

	return 0;
}

static u32 mtk_dsi_recv_cnt(u8 type, u8 *read_data)
{
	switch (type) {
	case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_1BYTE:
	case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_1BYTE:
		return 1;
	case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_2BYTE:
	case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_2BYTE:
		return 2;
	case MIPI_DSI_RX_GENERIC_LONG_READ_RESPONSE:
	case MIPI_DSI_RX_DCS_LONG_READ_RESPONSE:
		return read_data[1] + read_data[2] * 16;
	case MIPI_DSI_RX_ACKNOWLEDGE_AND_ERROR_REPORT:
		DDPINFO("type is 0x02, try again\n");
		break;
	default:
		DDPINFO("type(0x%x) cannot be non-recognite\n", type);
		break;
	}

	return 0;
}

static void mtk_dsi_cmdq(struct mtk_dsi *dsi, const struct mipi_dsi_msg *msg)
{
	const char *tx_buf = msg->tx_buf;
	u8 config, cmdq_size, cmdq_off, type = msg->type;
	u32 reg_val, cmdq_mask, i;
	unsigned long goto_addr;

	if (MTK_DSI_HOST_IS_READ(type))
		config = BTA;
	else
		config = (msg->tx_len > 2) ? LONG_PACKET : SHORT_PACKET;

	if (msg->tx_len > 2) {
		cmdq_size = 1 + (msg->tx_len + 3) / 4;
		cmdq_off = 4;
		cmdq_mask = CONFIG | DATA_ID | DATA_0 | DATA_1;
		reg_val = (msg->tx_len << 16) | (type << 8) | config;
	} else {
		cmdq_size = 1;
		cmdq_off = 2;
		cmdq_mask = CONFIG | DATA_ID;
		reg_val = (type << 8) | config;
	}

	for (i = 0; i < msg->tx_len; i++) {
		goto_addr = dsi->driver_data->reg_cmdq_ofs + cmdq_off + i;
		cmdq_mask = (0xFFu << ((goto_addr & 0x3u) * 8));
		mtk_dsi_mask(dsi, goto_addr & (~(0x3UL)),
			     (0xFFu << ((goto_addr & 0x3u) * 8)),
			     tx_buf[i] << ((goto_addr & 0x3u) * 8));
	}
	if (msg->tx_len > 2)
		cmdq_mask = CONFIG | DATA_ID | DATA_0 | DATA_1;
	else
		cmdq_mask = CONFIG | DATA_ID;

	mtk_dsi_mask(dsi, dsi->driver_data->reg_cmdq_ofs, cmdq_mask, reg_val);
	mtk_dsi_mask(dsi, DSI_CMDQ_SIZE, CMDQ_SIZE, cmdq_size);
}

static void build_vm_cmdq(struct mtk_dsi *dsi,
	const struct mipi_dsi_msg *msg, struct cmdq_pkt *handle)
{
	unsigned int i = 0, j = 0, k;
	const char *tx_buf = msg->tx_buf;

	while (i < msg->tx_len) {
		unsigned int vm_cmd_val  = 0;
		unsigned int vm_cmd_addr  = 0;

		k = (((j + 4) > msg->tx_len) ? (msg->tx_len) : (j + 4));
		for (j = i; j < k; j++)
			vm_cmd_val += (tx_buf[j] << ((j - i) * 8));

		if (i / 16 == 0)
			vm_cmd_addr = DSI_VM_CMD_DATA0  + (i%16);
		if (i / 16 == 1)
			vm_cmd_addr = DSI_VM_CMD_DATA10 + (i%16);
		if (i / 16 == 2)
			vm_cmd_addr = DSI_VM_CMD_DATA20 + (i%16);
		if (i / 16 == 3)
			vm_cmd_addr = DSI_VM_CMD_DATA30 + (i%16);

		if (handle)
			cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
				dsi->ddp_comp.regs_pa + vm_cmd_addr,
				vm_cmd_val, ~0);
		else
			writel(vm_cmd_val, dsi->regs + vm_cmd_addr);

		i += 4;
	}
}

static void mtk_dsi_vm_cmdq(struct mtk_dsi *dsi,
	const struct mipi_dsi_msg *msg, struct cmdq_pkt *handle)
{
	const char *tx_buf = msg->tx_buf;
	u8 config, type = msg->type;
	u32 reg_val;

	config = (msg->tx_len > 2) ? VM_LONG_PACKET : 0;

	if (msg->tx_len > 2) {
		build_vm_cmdq(dsi, msg, handle);
		reg_val = (msg->tx_len << 16) | (type << 8) | config;
	} else if (msg->tx_len == 2) {
		reg_val = (tx_buf[1] << 24) | (tx_buf[0] << 16) |
			(type << 8) | config;
	} else {
		reg_val = (tx_buf[0] << 16) | (type << 8) | config;
	}

	reg_val |= (VM_CMD_EN + TS_VFP_EN);

	if (handle == NULL)
		writel(reg_val, dsi->regs + DSI_VM_CMD_CON);
	else
		cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
			dsi->ddp_comp.regs_pa + DSI_VM_CMD_CON, reg_val, ~0);

}

static void mtk_dsi_cmdq_gce(struct mtk_dsi *dsi, struct cmdq_pkt *handle,
				const struct mipi_dsi_msg *msg)
{
	const char *tx_buf = msg->tx_buf;
	u8 config, cmdq_size, cmdq_off, type = msg->type;
	u32 reg_val, cmdq_mask, i;
	unsigned long goto_addr;

	if (MTK_DSI_HOST_IS_READ(type))
		config = BTA;
	else
		config = (msg->tx_len > 2) ? LONG_PACKET : SHORT_PACKET;

	if (msg->tx_len > 2) {
		cmdq_size = 1 + (msg->tx_len + 3) / 4;
		cmdq_off = 4;
		cmdq_mask = CONFIG | DATA_ID | DATA_0 | DATA_1;
		reg_val = (msg->tx_len << 16) | (type << 8) | config;
	} else {
		cmdq_size = 1;
		cmdq_off = 2;
		cmdq_mask = CONFIG | DATA_ID;
		reg_val = (type << 8) | config;
	}

	for (i = 0; i < msg->tx_len; i++) {
		goto_addr = dsi->driver_data->reg_cmdq_ofs + cmdq_off + i;
		cmdq_mask = (0xFFu << ((goto_addr & 0x3u) * 8));
		mtk_ddp_write_mask(&dsi->ddp_comp,
			tx_buf[i] << ((goto_addr & 0x3u) * 8),
			goto_addr, (0xFFu << ((goto_addr & 0x3u) * 8)),
			handle);

		DDPINFO("set cmdqaddr %lx, val:%x, mask %x\n", goto_addr,
			tx_buf[i] << ((goto_addr & 0x3u) * 8),
			(0xFFu << ((goto_addr & 0x3u) * 8)));
	}
	if (msg->tx_len > 2)
		cmdq_mask = CONFIG | DATA_ID | DATA_0 | DATA_1;
	else
		cmdq_mask = CONFIG | DATA_ID;

	mtk_ddp_write_mask(&dsi->ddp_comp, reg_val,
				dsi->driver_data->reg_cmdq_ofs,
				cmdq_mask, handle);
	DDPINFO("set cmdqaddr %u, val:%x, mask %x\n",
			dsi->driver_data->reg_cmdq_ofs,
			reg_val,
			cmdq_mask);
	mtk_ddp_write_mask(&dsi->ddp_comp, cmdq_size,
				DSI_CMDQ_SIZE, CMDQ_SIZE, handle);
	DDPINFO("set cmdqaddr %u, val:%x, mask %x\n", DSI_CMDQ_SIZE, cmdq_size,
			CMDQ_SIZE);
}

static void mtk_dsi_cmdq_grp_gce(struct mtk_dsi *dsi, struct cmdq_pkt *handle,
				struct mtk_panel_para_table *para_table,
				unsigned int para_size)
{
	struct mipi_dsi_msg msg;
	const char *tx_buf;
	u8 config, cmdq_off, type;
	u8 cmdq_size, total_cmdq_size = 0;
	u8 start_off = 0;
	u32 reg_val, cmdq_val;
	u32 cmdq_mask, i, j;
	unsigned int base_addr;
	struct mtk_ddp_comp *comp = &dsi->ddp_comp;
	const u32 reg_cmdq_ofs = dsi->driver_data->reg_cmdq_ofs;

	for (j = 0; j < para_size; j++) {
		msg.tx_buf = para_table[j].para_list,
		msg.tx_len = para_table[j].count;

		switch (msg.tx_len) {
		case 0:
			continue;

		case 1:
			msg.type = MIPI_DSI_DCS_SHORT_WRITE;
			break;

		case 2:
			msg.type = MIPI_DSI_DCS_SHORT_WRITE_PARAM;
			break;

		default:
			msg.type = MIPI_DSI_DCS_LONG_WRITE;
			break;
		}

		tx_buf = msg.tx_buf;
		type = msg.type;

		if (MTK_DSI_HOST_IS_READ(type))
			config = BTA;
		else
			config = (msg.tx_len > 2) ? LONG_PACKET : SHORT_PACKET;

		if (msg.tx_len > 2) {
			cmdq_off = 4;
			cmdq_mask = CONFIG | DATA_ID | DATA_0 | DATA_1;
			reg_val = (msg.tx_len << 16) | (type << 8) | config;

			mtk_ddp_write_relaxed(comp, reg_val,
						reg_cmdq_ofs + start_off,
						handle);
			DDPINFO("set cmdq addr %x, val:%x\n",
					reg_cmdq_ofs + start_off,
					reg_val);

			reg_val = 0;
			for (i = 0; i < msg.tx_len; i++) {
				cmdq_val = tx_buf[i] << ((i & 0x3u) * 8);
				cmdq_mask = (0xFFu << ((i & 0x3u) * 8));
				reg_val = reg_val | (cmdq_val & cmdq_mask);

				if (((i & 0x3) == 0x3) ||
					(i == (msg.tx_len - 1))) {
					base_addr = reg_cmdq_ofs + start_off +
						cmdq_off + ((i / 4) * 4);
					mtk_ddp_write_relaxed(comp,
						reg_val,
						base_addr,
						handle);

					DDPINFO("set cmdq addr %x, val:%x\n",
						base_addr,
						reg_val);
					reg_val = 0;
				}
			}
		} else {
			cmdq_off = 2;
			cmdq_mask = CONFIG | DATA_ID;
			reg_val = (type << 8) | config;

			for (i = 0; i < msg.tx_len; i++) {
				cmdq_val = tx_buf[i] << ((i & 0x3u) * 8);
				cmdq_mask = (0xFFu << ((i & 0x3u) * 8));
				reg_val = reg_val | (cmdq_val & cmdq_mask);

				if (i == (msg.tx_len - 1)) {
					base_addr = reg_cmdq_ofs + start_off +
						cmdq_off + (i / 4) * 4;
					mtk_ddp_write_relaxed(comp,
						reg_val,
						base_addr,
						handle);

					DDPINFO("set cmdq addr %x, val:%x\n",
						base_addr,
						reg_val);
					reg_val = 0;
				}
			}
		}

		if (msg.tx_len > 2)
			cmdq_size = 1 + ((msg.tx_len + 3) / 4);
		else
			cmdq_size = 1;

		start_off += (cmdq_size * 4);
		total_cmdq_size += cmdq_size;
		DDPINFO("offset:%d, size:%d\n", start_off, cmdq_size);
	}

	mtk_ddp_write_mask(comp, total_cmdq_size,
				DSI_CMDQ_SIZE, CMDQ_SIZE, handle);

	mtk_ddp_write_relaxed(comp, 0x0, DSI_START, handle);
	mtk_ddp_write_relaxed(comp, 0x1, DSI_START, handle);
	mtk_dsi_cmdq_poll(comp, handle, comp->regs_pa + DSI_INTSTA,
			CMD_DONE_INT_FLAG, CMD_DONE_INT_FLAG);
	mtk_ddp_write_mask(comp, 0x0, DSI_INTSTA, CMD_DONE_INT_FLAG,
			handle);

	DDPINFO("set cmdqaddr %x, val:%d, mask %x\n", DSI_CMDQ_SIZE,
			total_cmdq_size,
			CMDQ_SIZE);
}

void mipi_dsi_dcs_write_gce(struct mtk_dsi *dsi, struct cmdq_pkt *handle,
				  const void *data, size_t len)
{
	struct mipi_dsi_msg msg = {
		.tx_buf = data,
		.tx_len = len
	};

	switch (len) {
	case 0:
		return;

	case 1:
		msg.type = MIPI_DSI_DCS_SHORT_WRITE;
		break;

	case 2:
		msg.type = MIPI_DSI_DCS_SHORT_WRITE_PARAM;
		break;

	default:
		msg.type = MIPI_DSI_DCS_LONG_WRITE;
		break;
	}

	if (mtk_dsi_is_cmd_mode(&dsi->ddp_comp)) {
		mtk_dsi_poll_for_idle(dsi, handle);
		mtk_dsi_cmdq_gce(dsi, handle, &msg);

		cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
			dsi->ddp_comp.regs_pa + DSI_START, 0x0, ~0);
		cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
			dsi->ddp_comp.regs_pa + DSI_START, 0x1, ~0);

		mtk_dsi_poll_for_idle(dsi, handle);
	} else {
		/* set BL cmd */
		mtk_dsi_vm_cmdq(dsi, &msg, handle);

		/* clear VM_CMD_DONE */
		cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
			dsi->ddp_comp.regs_pa + DSI_INTSTA, 0,
			VM_CMD_DONE_INT_EN);

		/* start to send VM cmd */
		cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
			dsi->ddp_comp.regs_pa + DSI_START, 0,
			VM_CMD_START);
		cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
			dsi->ddp_comp.regs_pa + DSI_START, VM_CMD_START,
			VM_CMD_START);

		/* poll VM cmd done */
		mtk_dsi_cmdq_poll(&dsi->ddp_comp, handle,
			dsi->ddp_comp.regs_pa + DSI_INTSTA,
			VM_CMD_DONE_INT_EN, VM_CMD_DONE_INT_EN);
	}
}

void mipi_dsi_dcs_write_gce_dyn(struct mtk_dsi *dsi, struct cmdq_pkt *handle,
				  const void *data, size_t len)
{
	struct mipi_dsi_msg msg = {
		.tx_buf = data,
		.tx_len = len
	};

	switch (len) {
	case 0:
		return;

	case 1:
		msg.type = MIPI_DSI_DCS_SHORT_WRITE;
		break;

	case 2:
		msg.type = MIPI_DSI_DCS_SHORT_WRITE_PARAM;
		break;

	default:
		msg.type = MIPI_DSI_DCS_LONG_WRITE;
		break;
	}

	mtk_dsi_poll_for_idle(dsi, handle);
	mtk_dsi_cmdq_gce(dsi, handle, &msg);

	cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
		dsi->ddp_comp.regs_pa + DSI_START, 0x0, ~0);
	cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
		dsi->ddp_comp.regs_pa + DSI_START, 0x1, ~0);

	mtk_dsi_poll_for_idle(dsi, handle);
}

void mipi_dsi_dcs_write_gce2(struct mtk_dsi *dsi, struct cmdq_pkt *dummy,
					  const void *data, size_t len)
{

	struct cmdq_pkt *handle;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(dsi->encoder.crtc);
	int dsi_mode = readl(dsi->regs + DSI_MODE_CTRL);

	struct mipi_dsi_msg msg = {
		.tx_buf = data,
		.tx_len = len
	};

	switch (len) {
	case 0:
		return;

	case 1:
		msg.type = MIPI_DSI_DCS_SHORT_WRITE;
		break;

	case 2:
		msg.type = MIPI_DSI_DCS_SHORT_WRITE_PARAM;
		break;

	default:
		msg.type = MIPI_DSI_DCS_LONG_WRITE;
		break;
	}

	if (dsi_mode == 0) {
		mtk_crtc_pkt_create(&handle, &mtk_crtc->base,
			mtk_crtc->gce_obj.client[CLIENT_DSI_CFG]);

		mtk_dsi_poll_for_idle(dsi, handle);

		mtk_dsi_cmdq_gce(dsi, handle, &msg);

		cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
			dsi->ddp_comp.regs_pa + DSI_START, 0x0, ~0);
		cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
			dsi->ddp_comp.regs_pa + DSI_START, 0x1, ~0);

		mtk_dsi_poll_for_idle(dsi, handle);
	} else {
		mtk_crtc_pkt_create(&handle, &mtk_crtc->base,
			mtk_crtc->gce_obj.client[CLIENT_DSI_CFG]);

		/* build VM cmd */
		mtk_dsi_vm_cmdq(dsi, &msg, handle);

		/* clear VM_CMD_DONE */
		cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
			dsi->ddp_comp.regs_pa + DSI_INTSTA, 0,
			VM_CMD_DONE_INT_EN);

		/* start to send VM cmd */
		cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
			dsi->ddp_comp.regs_pa + DSI_START, 0,
			VM_CMD_START);
		cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
			dsi->ddp_comp.regs_pa + DSI_START, VM_CMD_START,
			VM_CMD_START);

		/* poll VM cmd done */
		mtk_dsi_cmdq_poll(&dsi->ddp_comp, handle,
			dsi->ddp_comp.regs_pa + DSI_INTSTA,
			VM_CMD_DONE_INT_EN, VM_CMD_DONE_INT_EN);

		cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
			dsi->ddp_comp.regs_pa + DSI_START, 0,
			VM_CMD_START);

		/* clear VM_CMD_DONE */
		cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
			dsi->ddp_comp.regs_pa + DSI_INTSTA, 0,
			VM_CMD_DONE_INT_EN);
	}

	cmdq_pkt_flush(handle);
	cmdq_pkt_destroy(handle);
}

void mipi_dsi_dcs_grp_write_gce(struct mtk_dsi *dsi, struct cmdq_pkt *handle,
				struct mtk_panel_para_table *para_table,
				unsigned int para_size)
{
	struct mtk_ddp_comp *comp = &dsi->ddp_comp;

	/* wait DSI idle */
	if (!mtk_dsi_is_cmd_mode(comp)) {
		_mtk_dsi_set_mode(comp, handle, CMD_MODE);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DSI_START, 0, ~0);
		mtk_dsi_cmdq_poll(comp, handle,
				  comp->regs_pa + DSI_INTSTA, 0,
				  DSI_BUSY);
	}

	mtk_dsi_cmdq_grp_gce(dsi, handle, para_table, para_size);

	/* trigger */
	if (!mtk_dsi_is_cmd_mode(comp)) {
		mtk_dsi_start_vdo_mode(comp, handle);
		mtk_disp_mutex_trigger(comp->mtk_crtc->mutex[0], handle);
		mtk_dsi_trigger(comp, handle);
	}
}

static void _mtk_mipi_dsi_write_gce(struct mtk_dsi *dsi,
				struct cmdq_pkt *handle,
				const struct mipi_dsi_msg *msg)
{
	const char *tx_buf = msg->tx_buf;
	u8 config, cmdq_size, cmdq_off, type = msg->type;
	u32 reg_val, cmdq_mask, i;
	unsigned long goto_addr;

	DDPMSG("%s +\n", __func__);

	if (MTK_DSI_HOST_IS_READ(type))
		config = BTA;
	else
		config = (msg->tx_len > 2) ? LONG_PACKET : SHORT_PACKET;
	DDPINFO("%s:%d config = %d\n", __func__, __LINE__, config);
	if (!(msg->flags & MIPI_DSI_MSG_USE_LPM)) {
		config |= HSTX;
		DDPINFO("%s:%d config = %d\n", __func__, __LINE__, config);
	}

	if (msg->tx_len > 2) {
		cmdq_size = 1 + (msg->tx_len + 3) / 4;
		cmdq_off = 4;
		cmdq_mask = CONFIG | DATA_ID | DATA_0 | DATA_1;
		reg_val = (msg->tx_len << 16) | (type << 8) | config;
	} else {
		cmdq_size = 1;
		cmdq_off = 2;
		cmdq_mask = CONFIG | DATA_ID;
		reg_val = (type << 8) | config;
	}

	for (i = 0; i < msg->tx_len; i++) {
		goto_addr = dsi->driver_data->reg_cmdq_ofs + cmdq_off + i;
		cmdq_mask = (0xFFu << ((goto_addr & 0x3u) * 8));
		mtk_ddp_write_mask(&dsi->ddp_comp,
			tx_buf[i] << ((goto_addr & 0x3u) * 8),
			goto_addr, (0xFFu << ((goto_addr & 0x3u) * 8)),
			handle);

		DDPINFO("set cmdqaddr %lx, val:%x, mask %x\n", goto_addr,
			tx_buf[i] << ((goto_addr & 0x3u) * 8),
			(0xFFu << ((goto_addr & 0x3u) * 8)));
	}
	if (msg->tx_len > 2)
		cmdq_mask = CONFIG | DATA_ID | DATA_0 | DATA_1;
	else
		cmdq_mask = CONFIG | DATA_ID;

	mtk_ddp_write_mask(&dsi->ddp_comp, reg_val,
				dsi->driver_data->reg_cmdq_ofs,
				cmdq_mask, handle);
	DDPINFO("set cmdqaddr %u, val:%x, mask %x\n",
			dsi->driver_data->reg_cmdq_ofs,
			reg_val,
			cmdq_mask);
	mtk_ddp_write_mask(&dsi->ddp_comp, cmdq_size,
				DSI_CMDQ_SIZE, CMDQ_SIZE, handle);
	DDPINFO("set cmdqaddr %u, val:%x, mask %x\n", DSI_CMDQ_SIZE, cmdq_size,
			CMDQ_SIZE);

	DDPMSG("%s -\n", __func__);
}

int mtk_mipi_dsi_write_gce(struct mtk_dsi *dsi,
			struct cmdq_pkt *handle,
			struct mtk_drm_crtc *mtk_crtc,
			struct mtk_ddic_dsi_msg *cmd_msg)
{
	unsigned int i = 0, j = 0;
	int dsi_mode = readl(dsi->regs + DSI_MODE_CTRL) & MODE;
	struct mipi_dsi_msg msg;
	unsigned int use_lpm = cmd_msg->flags & MIPI_DSI_MSG_USE_LPM;
	struct mtk_ddp_comp *comp = &dsi->ddp_comp;

	DDPINFO("%s +, dsi_mode = %d, use_lpm = %d\n", __func__, dsi_mode, use_lpm);

	/* Check cmd_msg param */
	if (cmd_msg->type == 0 ||
		cmd_msg->tx_cmd_num == 0 ||
		cmd_msg->tx_cmd_num > MAX_TX_CMD_NUM) {
		DDPPR_ERR("%s: type is %s, tx_cmd_num is %d\n",
			__func__, cmd_msg->type, (int)cmd_msg->tx_cmd_num);
		return -EINVAL;
	}

	for (i = 0; i < cmd_msg->tx_cmd_num; i++) {
		if (cmd_msg->tx_buf[i] == 0 || cmd_msg->tx_len[i] == 0) {
			DDPPR_ERR("%s: tx_buf[%d] is %s, tx_len[%d] is %d\n",
				__func__, i, (char *)cmd_msg->tx_buf[i], i,
				(int)cmd_msg->tx_len[i]);
			return -EINVAL;
		}
	}

	/* Debug info */
	DDPINFO("%s: channel=%d, flags=0x%x, tx_cmd_num=%d\n",
		__func__, cmd_msg->channel,
		cmd_msg->flags, (int)cmd_msg->tx_cmd_num);
	for (i = 0; i < cmd_msg->tx_cmd_num; i++) {
		DDPINFO("type[%d]=0x%x, tx_len[%d]=%d\n",
			i, cmd_msg->type[i], i, (int)cmd_msg->tx_len[i]);
		for (j = 0; j < cmd_msg->tx_len[i]; j++) {
			DDPINFO("tx_buf[%d]--byte:%d,val:0x%x\n",
				i, j, *(char *)(cmd_msg->tx_buf[i] + j));
		}
	}

	msg.channel = cmd_msg->channel;
	msg.flags = cmd_msg->flags;

	if (dsi_mode == 0) { /* CMD mode HS/LP */
		DDPINFO("%s:%d\n", __func__, __LINE__);
		for (i = 0; i < cmd_msg->tx_cmd_num; i++) {
			msg.type = cmd_msg->type[i];
			msg.tx_len = cmd_msg->tx_len[i];
			msg.tx_buf = cmd_msg->tx_buf[i];

			mtk_dsi_poll_for_idle(dsi, handle);

			_mtk_mipi_dsi_write_gce(dsi, handle, &msg);

			cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
				dsi->ddp_comp.regs_pa + DSI_START, 0x0, ~0);
			cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
				dsi->ddp_comp.regs_pa + DSI_START, 0x1, ~0);

			mtk_dsi_poll_for_idle(dsi, handle);
		}
	} else if (dsi_mode != 0 && !use_lpm) { /* VDO with VM_CMD */
		DDPINFO("%s:%d\n", __func__, __LINE__);
		for (i = 0; i < cmd_msg->tx_cmd_num; i++) {
			msg.type = cmd_msg->type[i];
			msg.tx_len = cmd_msg->tx_len[i];
			msg.tx_buf = cmd_msg->tx_buf[i];

			/* build VM cmd */
			mtk_dsi_vm_cmdq(dsi, &msg, handle);

			/* clear VM_CMD_DONE */
			cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
				dsi->ddp_comp.regs_pa + DSI_INTSTA, 0,
				VM_CMD_DONE_INT_EN);

			/* start to send VM cmd */
			cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
				dsi->ddp_comp.regs_pa + DSI_START, 0,
				VM_CMD_START);
			cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
				dsi->ddp_comp.regs_pa + DSI_START, VM_CMD_START,
				VM_CMD_START);

			/* poll VM cmd done */
			mtk_dsi_cmdq_poll(&dsi->ddp_comp, handle,
				dsi->ddp_comp.regs_pa + DSI_INTSTA,
				VM_CMD_DONE_INT_EN, VM_CMD_DONE_INT_EN);

			cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
				dsi->ddp_comp.regs_pa + DSI_START, 0,
				VM_CMD_START);

			/* clear VM_CMD_DONE */
			cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
				dsi->ddp_comp.regs_pa + DSI_INTSTA, 0,
				VM_CMD_DONE_INT_EN);
		}
	} else if (dsi_mode != 0 && use_lpm) { /* VDO to CMD with LP */
		mtk_dsi_stop_vdo_mode(dsi, handle);
		DDPINFO("%s:%d\n", __func__, __LINE__);
		for (i = 0; i < cmd_msg->tx_cmd_num; i++) {
			msg.type = cmd_msg->type[i];
			msg.tx_len = cmd_msg->tx_len[i];
			msg.tx_buf = cmd_msg->tx_buf[i];

			mtk_dsi_poll_for_idle(dsi, handle);

			_mtk_mipi_dsi_write_gce(dsi, handle, &msg);

			cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
				dsi->ddp_comp.regs_pa + DSI_START, 0x0, ~0);
			cmdq_pkt_write(handle, dsi->ddp_comp.cmdq_base,
				dsi->ddp_comp.regs_pa + DSI_START, 0x1, ~0);

			mtk_dsi_poll_for_idle(dsi, handle);
		}

		mtk_dsi_start_vdo_mode(comp, handle);
		mtk_disp_mutex_trigger(comp->mtk_crtc->mutex[0], handle);
		mtk_dsi_trigger(comp, handle);
	}

	DDPINFO("%s -\n", __func__);
	return 0;
}

static void _mtk_mipi_dsi_read_gce(struct mtk_dsi *dsi,
				struct cmdq_pkt *handle,
				struct mipi_dsi_msg *msg,
				unsigned int cmd_idx)
{
	struct mtk_ddp_comp *comp = &dsi->ddp_comp;
	struct mtk_drm_crtc *mtk_crtc = dsi->ddp_comp.mtk_crtc;
	struct DSI_T0_INS t0, t1;
	dma_addr_t read_slot = mtk_crtc->gce_obj.buf.pa_base +
					DISP_SLOT_READ_DDIC_BASE +
					cmd_idx * 0x10;
	const char *tx_buf = msg->tx_buf;

	DDPMSG("%s +\n", __func__);

	DDPINFO("%s type=0x%x, tx_len=%d, tx_buf[0]=0x%x, rx_len=%d\n",
		__func__, msg->type, (int)msg->tx_len,
		tx_buf[0], (int)msg->rx_len);

	if (msg->tx_len > 2) {
		DDPPR_ERR("%s: msg->tx_len is more than 2\n", __func__);
		goto done;
	}

	t0.CONFG = 0x00;
	t0.Data_ID = 0x37;
	t0.Data0 = msg->rx_len;
	t0.Data1 = 0;

	t1.CONFG = BTA;
	t1.Data_ID = msg->type;
	t1.Data0 = tx_buf[0];
	if (msg->tx_len == 2)
		t1.Data1 = tx_buf[1];
	else
		t1.Data1 = 0;

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DSI_CMDQ0,
		AS_UINT32(&t0), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DSI_CMDQ1,
		AS_UINT32(&t1), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DSI_CMDQ_SIZE,
		0x2, ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DSI_START,
		0x0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DSI_START,
		0x1, ~0);

	mtk_dsi_cmdq_poll(comp, handle, comp->regs_pa + DSI_INTSTA, 0x1, 0x1);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DSI_INTSTA,
		0x0, 0x1);

	cmdq_pkt_mem_move(handle, comp->cmdq_base,
		comp->regs_pa + DSI_RX_DATA0, read_slot,
		CMDQ_THR_SPR_IDX3);
	cmdq_pkt_mem_move(handle, comp->cmdq_base,
		comp->regs_pa + DSI_RX_DATA1, read_slot + 1 * 0x4,
		CMDQ_THR_SPR_IDX3);
	cmdq_pkt_mem_move(handle, comp->cmdq_base,
		comp->regs_pa + DSI_RX_DATA2, read_slot + 2 * 0x4,
		CMDQ_THR_SPR_IDX3);
	cmdq_pkt_mem_move(handle, comp->cmdq_base,
		comp->regs_pa + DSI_RX_DATA3, read_slot + 3 * 0x4,
		CMDQ_THR_SPR_IDX3);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DSI_RACK,
		0x1, 0x1);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DSI_INTSTA,
		0x0, 0x1);

	mtk_dsi_poll_for_idle(dsi, handle);

done:
	DDPMSG("%s -\n", __func__);
}

static unsigned int read_ddic_chk_sta;

static void ddic_read_timeout_cb(struct cmdq_cb_data data)
{
	struct drm_crtc *crtc = data.data;

	if (!crtc) {
		DDPPR_ERR("%s find crtc fail\n", __func__);
		return;
	}

	DDPPR_ERR("%s flush fail\n", __func__);
	read_ddic_chk_sta = 0xff;
	mtk_drm_crtc_analysis(crtc);
	mtk_drm_crtc_dump(crtc);
}

int mtk_mipi_dsi_read_gce(struct mtk_dsi *dsi,
			struct cmdq_pkt *handle,
			struct mtk_drm_crtc *mtk_crtc,
			struct mtk_ddic_dsi_msg *cmd_msg)
{
	unsigned int i = 0, j = 0;
	int dsi_mode = readl(dsi->regs + DSI_MODE_CTRL) & MODE;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mipi_dsi_msg msg;
	struct mtk_ddp_comp *comp = &dsi->ddp_comp;
	struct cmdq_pkt *cmdq_handle, *cmdq_handle2;
	int ret = 0;
	struct DSI_RX_DATA_REG read_data0 = {0, 0, 0, 0};
	struct DSI_RX_DATA_REG read_data1 = {0, 0, 0, 0};
	struct DSI_RX_DATA_REG read_data2 = {0, 0, 0, 0};
	struct DSI_RX_DATA_REG read_data3 = {0, 0, 0, 0};
	unsigned char packet_type;
	unsigned int recv_data_cnt = 0;

	DDPMSG("%s +\n", __func__);

	/* Check cmd_msg param */
	if (cmd_msg->type == 0 ||
		cmd_msg->tx_cmd_num == 0 ||
		cmd_msg->rx_cmd_num == 0 ||
		cmd_msg->tx_cmd_num > MAX_TX_CMD_NUM ||
		cmd_msg->rx_cmd_num > MAX_RX_CMD_NUM) {
		DDPPR_ERR(
			"%s: type is %s, tx_cmd_num is %d, rx_cmd_num is %d\n",
			__func__, cmd_msg->type,
			(int)cmd_msg->tx_cmd_num, (int)cmd_msg->rx_cmd_num);
		return -EINVAL;
	}

	if (cmd_msg->tx_cmd_num != cmd_msg->rx_cmd_num) {
		DDPPR_ERR("%s: tx_cmd_num is %d, rx_cmd_num is %d\n",
			__func__, (int)cmd_msg->tx_cmd_num,
			(int)cmd_msg->rx_cmd_num);
		return -EINVAL;
	}

	for (i = 0; i < cmd_msg->tx_cmd_num; i++) {
		if (cmd_msg->tx_buf[i] == 0 || cmd_msg->tx_len[i] == 0) {
			DDPPR_ERR("%s: tx_buf[%d] is %s, tx_len[%d] is %d\n",
				__func__, i, (char *)cmd_msg->tx_buf[i], i,
				(int)cmd_msg->tx_len[i]);
			return -EINVAL;
		}
	}

	for (i = 0; i < cmd_msg->rx_cmd_num; i++) {
		if (cmd_msg->rx_buf[i] == 0 || cmd_msg->rx_len[i] == 0) {
			DDPPR_ERR("%s: rx_buf[%d] is %s, rx_len[%d] is %d\n",
				__func__, i, (char *)cmd_msg->rx_buf[i], i,
				(int)cmd_msg->rx_len[i]);
			return -EINVAL;
		}

		if (cmd_msg->rx_len[i] > RT_MAX_NUM) {
			DDPPR_ERR("%s: only supprt read 10 bytes params\n",
				__func__);
			cmd_msg->rx_len[i] = RT_MAX_NUM;
		}
	}

	/* Debug info */
	DDPINFO("%s: channel=%d, flags=0x%x, tx_cmd_num=%d, rx_cmd_num=%d\n",
		__func__, cmd_msg->channel,
		cmd_msg->flags, (int)cmd_msg->tx_cmd_num,
		(int)cmd_msg->rx_cmd_num);

	for (i = 0; i < cmd_msg->tx_cmd_num; i++) {
		DDPINFO("type[%d]=0x%x, tx_len[%d]=%d\n",
			i, cmd_msg->type[i], i, (int)cmd_msg->tx_len[i]);
		for (j = 0; j < (int)cmd_msg->tx_len[i]; j++) {
			DDPINFO("tx_buf[%d]--byte:%d,val:0x%x\n",
				i, j, *(char *)(cmd_msg->tx_buf[i] + j));
		}
	}

	msg.channel = cmd_msg->channel;
	msg.flags = cmd_msg->flags;

	cmdq_handle = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_DSI_CFG]);
	cmdq_handle->err_cb.cb = ddic_read_timeout_cb;
	cmdq_handle->err_cb.data = crtc;

	/* Reset DISP_SLOT_READ_DDIC_BASE to 0xff00ff00 */
	for (i = 0; i < READ_DDIC_SLOT_NUM; i++) {
		cmdq_pkt_write(cmdq_handle,
			mtk_crtc->gce_obj.base,
			(mtk_crtc->gce_obj.buf.pa_base +
				DISP_SLOT_READ_DDIC_BASE + i * 0x4),
			0xff00ff00, ~0);
	}

	if (dsi_mode == 0) { /* CMD mode LP */
		cmdq_pkt_wait_no_clear(cmdq_handle,
				mtk_crtc->gce_obj.event[EVENT_STREAM_EOF]);

		cmdq_pkt_clear_event(cmdq_handle,
				mtk_crtc->gce_obj.event[EVENT_ESD_EOF]);
		mtk_dsi_poll_for_idle(dsi, cmdq_handle);

		for (i = 0; i < cmd_msg->rx_cmd_num; i++) {
			msg.type = cmd_msg->type[i];
			msg.tx_len = cmd_msg->tx_len[i];
			msg.tx_buf = cmd_msg->tx_buf[i];
			msg.rx_len = cmd_msg->rx_len[i];
			msg.rx_buf = cmd_msg->rx_buf[i];

			_mtk_mipi_dsi_read_gce(dsi, cmdq_handle, &msg, i);
		}
		cmdq_pkt_set_event(cmdq_handle,
				mtk_crtc->gce_obj.event[EVENT_ESD_EOF]);
	} else { /* VDO to CMD mode LP */
		cmdq_pkt_wfe(cmdq_handle,
				mtk_crtc->gce_obj.event[EVENT_VDO_EOF]);

		mtk_dsi_stop_vdo_mode(dsi, cmdq_handle);

		for (i = 0; i < cmd_msg->rx_cmd_num; i++) {
			msg.type = cmd_msg->type[i];
			msg.tx_len = cmd_msg->tx_len[i];
			msg.tx_buf = cmd_msg->tx_buf[i];
			msg.rx_len = cmd_msg->rx_len[i];
			msg.rx_buf = cmd_msg->rx_buf[i];

			_mtk_mipi_dsi_read_gce(dsi, cmdq_handle, &msg, i);
		}
		mtk_dsi_start_vdo_mode(comp, cmdq_handle);
		mtk_disp_mutex_trigger(comp->mtk_crtc->mutex[0], cmdq_handle);
		mtk_dsi_trigger(comp, cmdq_handle);
	}

	read_ddic_chk_sta = 0;
	cmdq_pkt_flush(cmdq_handle);

	mtk_dsi_clear_rxrd_irq(dsi);

	if (read_ddic_chk_sta == 0xff) {
		ret = -EINVAL;
		/* CMD mode error handle */
		if (dsi_mode == 0) {
			/* TODO: set ESD_EOF event through CPU is better */
			mtk_crtc_pkt_create(&cmdq_handle2, crtc,
				mtk_crtc->gce_obj.client[CLIENT_DSI_CFG]);

			cmdq_pkt_set_event(
				cmdq_handle2,
				mtk_crtc->gce_obj.event[EVENT_ESD_EOF]);
			cmdq_pkt_flush(cmdq_handle2);
			cmdq_pkt_destroy(cmdq_handle2);
		}
		goto done;
	}

	for (i = 0; i < cmd_msg->rx_cmd_num; i++) {
		/* Copy slot data to data array */
		memcpy((void *)&read_data0,
			(mtk_crtc->gce_obj.buf.va_base +
				DISP_SLOT_READ_DDIC_BASE + (0 + i * 0x4) * 0x4),
				sizeof(unsigned int));
		memcpy((void *)&read_data1,
			(mtk_crtc->gce_obj.buf.va_base +
				DISP_SLOT_READ_DDIC_BASE + (1 + i * 0x4) * 0x4),
				sizeof(unsigned int));
		memcpy((void *)&read_data2,
			(mtk_crtc->gce_obj.buf.va_base +
				DISP_SLOT_READ_DDIC_BASE + (2 + i * 0x4) * 0x4),
				sizeof(unsigned int));
		memcpy((void *)&read_data3,
			(mtk_crtc->gce_obj.buf.va_base +
				DISP_SLOT_READ_DDIC_BASE + (3 + i * 0x4) * 0x4),
				sizeof(unsigned int));

		DDPINFO("%s:cmd_idx(%d) readback\n", __func__, i);
		DDPINFO("%s: read_data0 byte0~3=0x%x~0x%x~0x%x~0x%x\n",
			__func__, read_data0.byte0, read_data0.byte1
			, read_data0.byte2, read_data0.byte3);
		DDPINFO("%s: read_data1 byte0~3=0x%x~0x%x~0x%x~0x%x\n",
			__func__, read_data1.byte0, read_data1.byte1
			, read_data1.byte2, read_data1.byte3);
		DDPINFO("%s: read_data2 byte0~3=0x%x~0x%x~0x%x~0x%x\n",
			__func__, read_data2.byte0, read_data2.byte1
			, read_data2.byte2, read_data2.byte3);
		DDPINFO("%s: read_data3 byte0~3=0x%x~0x%x~0x%x~0x%x\n",
			__func__, read_data3.byte0, read_data3.byte1
			, read_data3.byte2, read_data3.byte3);

		/*parse packet*/
		packet_type = read_data0.byte0;
			/* 0x02: acknowledge & error report */
			/* 0x11: generic short read response(1 byte return) */
			/* 0x12: generic short read response(2 byte return) */
			/* 0x1a: generic long read response */
			/* 0x1c: dcs long read response */
			/* 0x21: dcs short read response(1 byte return) */
			/* 0x22: dcs short read response(2 byte return) */
		if (packet_type == 0x1A || packet_type == 0x1C) {
			recv_data_cnt = read_data0.byte1
					+ read_data0.byte2 * 16;

			if (recv_data_cnt > RT_MAX_NUM) {
				DDPMSG("DSI read long packet > 10 bytes\n");
					recv_data_cnt = RT_MAX_NUM;
			}
			if (recv_data_cnt > cmd_msg->rx_len[i])
				recv_data_cnt = cmd_msg->rx_len[i];

			DDPINFO("DSI read long packet size: %d\n",
				recv_data_cnt);
			if (recv_data_cnt <= 4) {
				memcpy((void *)cmd_msg->rx_buf[i],
					(void *)&read_data1, recv_data_cnt);
			} else if (recv_data_cnt <= 8) {
				memcpy((void *)cmd_msg->rx_buf[i],
					(void *)&read_data1, 4);
				memcpy((void *)(cmd_msg->rx_buf[i] + 4),
					(void *)&read_data2, recv_data_cnt - 4);
			} else {
				memcpy((void *)cmd_msg->rx_buf[i],
						(void *)&read_data1, 4);
				memcpy((void *)(cmd_msg->rx_buf[i] + 4),
						(void *)&read_data2, 4);
				memcpy((void *)(cmd_msg->rx_buf[i] + 8),
					(void *)&read_data3, recv_data_cnt - 8);
			}

		} else if (packet_type == 0x11 || packet_type == 0x21) {
			recv_data_cnt = 1;
			memcpy((void *)cmd_msg->rx_buf[i],
				(void *)&read_data0.byte1, recv_data_cnt);

		} else if (packet_type == 0x12 || packet_type == 0x22) {
			recv_data_cnt = 2;
			if (recv_data_cnt > cmd_msg->rx_len[i])
				recv_data_cnt = cmd_msg->rx_len[i];

			memcpy((void *)cmd_msg->rx_buf[i],
				(void *)&read_data0.byte1, recv_data_cnt);

		} else if (packet_type == 0x02) {
			DDPINFO("read return type is 0x02, re-read\n");
		} else {
			DDPINFO("return faulty type, type = 0x%x\n",
					packet_type);
		}
		msg.rx_len = recv_data_cnt;
		DDPINFO("[DSI]packet_type~recv_data_cnt = 0x%x~0x%x\n",
				packet_type, recv_data_cnt);
#if 0
		/* Todo: Support read multiple registers */
		cmd_msg->rx_len[0] = msg.rx_len;
		cmd_msg->rx_buf[0] = msg.rx_buf;
#endif
	}
		/* Debug info */
	for (i = 0; i < cmd_msg->rx_cmd_num; i++) {
		DDPINFO("rx_len[%d]=%d\n", i, (int)cmd_msg->rx_len[i]);
		for (j = 0; j < cmd_msg->rx_len[i]; j++) {
			DDPINFO("rx_buf[%d]--byte:%d,val:0x%x\n",
				i, j, *(char *)(cmd_msg->rx_buf[i] + j));
		}
	}

done:
	cmdq_pkt_destroy(cmdq_handle);

	DDPMSG("%s -\n", __func__);
	return 0;
}

static char string_to_hex(const char *str)
{
	char val_l = 0;
	char val_h = 0;

	if (str[0] >= '0' && str[0] <= '9')
		val_h = str[0] - '0';
	else if (str[0] <= 'f' && str[0] >= 'a')
		val_h = 10 + str[0] - 'a';
	else if (str[0] <= 'F' && str[0] >= 'A')
		val_h = 10 + str[0] - 'A';

	if (str[1] >= '0' && str[1] <= '9')
		val_l = str[1]-'0';
	else if (str[1] <= 'f' && str[1] >= 'a')
		val_l = 10 + str[1] - 'a';
	else if (str[1] <= 'F' && str[1] >= 'A')
		val_l = 10 + str[1] - 'A';

	return (val_h << 4) | val_l;
}

static int string_merge_into_buf(const char *str, int len, char *buf)
{
	int buf_size = 0;
	int i = 0;
	const char *p = str;

	while (i < len) {
		if (((p[0] >= '0' && p[0] <= '9') ||
			(p[0] <= 'f' && p[0] >= 'a') ||
			(p[0] <= 'F' && p[0] >= 'A'))
			&& ((i + 1) < len)) {
			buf[buf_size] = string_to_hex(p);
			pr_debug("0x%02x ", buf[buf_size]);
			buf_size++;
			i += 2;
			p += 2;
		} else {
			i++;
			p++;
		}
	}
	return buf_size;
}

#define to_mtk_dsi(x)  container_of(x, struct mtk_dsi, conn)
int dsi_display_set_doze_brightness(struct drm_connector *connector,
			int doze_brightness)
{
	int ret = 0;
	struct mtk_dsi *dsi = NULL;
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	pr_info("%s +\n", __func__);
	dsi = (struct mtk_dsi *)to_mtk_dsi(connector);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	mutex_lock(&dsi->dsi_lock);
	pr_info("%s mutex lock\n", __func__);
	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->set_doze_brightness)) {
		pr_info("%s set_doze_brightness func not defined");
		ret = 0;
	} else {
		ret = panel_ext->funcs->set_doze_brightness(dsi->panel, doze_brightness);
	}

	pr_info("%s mutex unlock\n", __func__);
	mutex_unlock(&dsi->dsi_lock);
	return ret;
}

ssize_t dsi_display_get_doze_brightness(struct drm_connector *connector,
			char *buf)
{
	int ret = 0;
	struct mtk_dsi *dsi = NULL;
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	pr_info("%s +\n", __func__);
	dsi = (struct mtk_dsi *)to_mtk_dsi(connector);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	mutex_lock(&dsi->dsi_lock);
	pr_info("%s mutex lock\n", __func__);
	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->get_doze_brightness)) {
		pr_info("%s get_doze_brightness func not defined");
		ret = 0;
	} else {
		ret = panel_ext->funcs->get_doze_brightness(dsi->panel, buf);
	}

	pr_info("%s mutex unlock\n", __func__);
	mutex_unlock(&dsi->dsi_lock);
	return 0;
}

static int mtk_dsi_io_cmd_dispparam(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle, enum DISPPARAM_MODE cmd, void *params);

int dsi_display_panel_set_thermal_hbm_disabled(struct drm_connector *connector,
			bool thermal_hbm_disabled)
{
	int ret=  0;
	struct mtk_dsi *dsi = NULL;
	struct mtk_ddp_comp *comp =  NULL;
	struct cmdq_pkt *cmdq_handle = NULL;
	uint32_t thermal_call = true;

	dsi = (struct mtk_dsi *)to_mtk_dsi(connector);
	comp = &dsi->ddp_comp;

	pr_info("%s, thermal_hbm_disabled:%x, hbm:%x\n", __func__,
		thermal_hbm_disabled, dsi->normal_hbm_flag);

	if (thermal_hbm_disabled) {
		mtk_dsi_io_cmd_dispparam(comp, cmdq_handle, DISPPARAM_SET_THERMAL_HBM_DISABLE, &thermal_call);
		if (dsi->normal_hbm_flag)
			mtk_dsi_io_cmd_dispparam(comp, cmdq_handle, DISPPARAM_HBM_OFF, &thermal_call);
	} else {
		mtk_dsi_io_cmd_dispparam(comp, cmdq_handle, DISPPARAM_CLEAR_THERMAL_HBM_DISABLE, &thermal_call);
		if (dsi->normal_hbm_flag)
			mtk_dsi_io_cmd_dispparam(comp, cmdq_handle, DISPPARAM_HBM_ON, &thermal_call);
	}

	return ret;
}

int dsi_display_panel_get_thermal_hbm_disabled(struct drm_connector *connector,
			bool *thermal_hbm_disabled)
{
	struct mtk_dsi *dsi = NULL;
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	dsi = (struct mtk_dsi *)to_mtk_dsi(connector);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	*thermal_hbm_disabled = dsi->thermal_hbm_disabled;
	pr_info("%s firworks thermal_hbm_disabled:%x \n", __func__, dsi->thermal_hbm_disabled);

	return 0;
}

ssize_t dsi_display_get_panel_info(struct drm_connector *connector,
			char *buf)
{
	struct mtk_dsi *dsi = NULL;
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	pr_info("%s +\n", __func__);
	dsi = (struct mtk_dsi *)to_mtk_dsi(connector);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->get_panel_info)) {
		pr_info("%s get_panel_info func not defined");
		return 0;
	} else {
		return panel_ext->funcs->get_panel_info(dsi->panel, buf);
	}
}

long lcm_mipi_reg_write(char *buf, size_t count)
{
	int retval = 0;
	int dlen = 0;
	unsigned int read_enable = 0;
	unsigned int packet_count = 0;
	unsigned int register_value = 0;
	char *input = NULL;
	char *data = NULL;
	unsigned char pbuf[3] = {0};
	u8 tx[10] = {0};
	unsigned int  i = 0, j = 0;
	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	pr_info("[%s]: mipi_write_date source: count = %d,buf = %s ", __func__, (int)count, buf);

	input = buf;
	memcpy(pbuf, input, 2);
	pbuf[2] = '\0';
	retval = kstrtou32(pbuf, 10, &read_enable);
	if (retval)
		goto exit;
	lcm_mipi_read_write.read_enable = !!read_enable;
	input = input + 3;
	memcpy(pbuf, input, 2);
	pbuf[2] = '\0';
	packet_count = (unsigned int)string_to_hex(pbuf);
	if (lcm_mipi_read_write.read_enable && !packet_count) {
		retval = -EINVAL;
		goto exit;
	}
	input = input + 3;
	memcpy(pbuf, input, 2);
	pbuf[2] = '\0';
	register_value = (unsigned int)string_to_hex(pbuf);
	lcm_mipi_read_write.lcm_setting_table.cmd = register_value;

	if(lcm_mipi_read_write.read_enable) {
		lcm_mipi_read_write.read_count = packet_count;

		cmd_msg->channel = 0;
		cmd_msg->tx_cmd_num = 1;
		cmd_msg->type[0] = 0x06;
		tx[0] = lcm_mipi_read_write.lcm_setting_table.cmd;
		cmd_msg->tx_buf[0] = tx;
		cmd_msg->tx_len[0] = 1;

		cmd_msg->rx_cmd_num = 1;
		cmd_msg->rx_buf[0] = lcm_mipi_read_write.read_buffer;
		memset(cmd_msg->rx_buf[0], 0, lcm_mipi_read_write.read_count);
		cmd_msg->rx_len[0] = lcm_mipi_read_write.read_count;
		retval = mtk_ddic_dsi_read_cmd(cmd_msg);
		if (retval != 0) {
			pr_err("%s error\n", __func__);
		}

		pr_info("read lcm addr:%pad--dlen:%d\n",
			&(*(char *)(cmd_msg->tx_buf[0])), (int)cmd_msg->rx_len[0]);
		for (j = 0; j < cmd_msg->rx_len[0]; j++) {
			pr_info("read lcm addr:%pad--byte:%d,val:%pad\n",
				&(*(char *)(cmd_msg->tx_buf[0])), j,
				&(*(char *)(cmd_msg->rx_buf[0] + j)));
		}
		goto exit;
	} else {
		lcm_mipi_read_write.lcm_setting_table.count = (unsigned char)packet_count;
		memcpy(lcm_mipi_read_write.lcm_setting_table.para_list, "",64);
		if(count > 8)
		{
			data = kzalloc(count - 6, GFP_KERNEL);
			if (!data) {
				retval = -ENOMEM;
				goto exit;
			}
			data[count-6-1] = '\0';
			//input = input + 3;
			dlen = string_merge_into_buf(input,count -6,data);
			memcpy(lcm_mipi_read_write.lcm_setting_table.para_list, data,dlen);

			cmd_msg->channel = packet_count;
			cmd_msg->flags = MIPI_DSI_MSG_USE_LPM;
			cmd_msg->tx_cmd_num = 1;
			cmd_msg->type[0] = 0x39;

			if (2 == dlen) {
				cmd_msg->type[0] = 0x15;
			} else if (1 == dlen) {
				cmd_msg->type[0] = 0x05;
			}

			cmd_msg->tx_buf[0] = data;
			cmd_msg->tx_len[0] = dlen;
			for (i = 0; i < (int)cmd_msg->tx_cmd_num; i++) {
				pr_debug("send lcm tx_len[%d]=%d\n",
					i, (int)cmd_msg->tx_len[i]);
				for (j = 0; j < (int)cmd_msg->tx_len[i]; j++) {
					pr_debug(
						"send lcm type[%d]=0x%x, tx_buf[%d]--byte:%d,val:%pad\n",
						i, cmd_msg->type[i], i, j,
						&(*(char *)(cmd_msg->tx_buf[i] + j)));
				}
			}

			mtk_ddic_dsi_send_cmd(cmd_msg, true, false);
		}
	}

	pr_debug("[%s]: mipi_write done!\n", __func__);
	pr_debug("[%s]: write cmd = %d,len = %d\n", __func__,lcm_mipi_read_write.lcm_setting_table.cmd,lcm_mipi_read_write.lcm_setting_table.count);
	pr_debug("[%s]: mipi_write data: ", __func__);
	for(i=0; i<count-3; i++)
	{
		pr_debug("0x%x ", lcm_mipi_read_write.lcm_setting_table.para_list[i]);
	}
	pr_debug("\n ");

	if(count > 8)
	{
		kfree(data);
	}
exit:
	retval = count;
	vfree(cmd_msg);
	return retval;
}


long  lcm_mipi_reg_read(char *buf)
{
	int i = 0;
	ssize_t count = 0;

	if (lcm_mipi_read_write.read_enable) {
		for (i = 0; i < lcm_mipi_read_write.read_count; i++) {
			if (i ==  lcm_mipi_read_write.read_count - 1) {
				count += snprintf(buf + count, PAGE_SIZE - count, "0x%02x\n",
				     lcm_mipi_read_write.read_buffer[i]);
			} else {
				count += snprintf(buf + count, PAGE_SIZE - count, "0x%02x ",
				     lcm_mipi_read_write.read_buffer[i]);
			}
		}
	}
	return count;
}

extern struct mtk_drm_crtc *g_mtk_crtc;
ssize_t panel_disp_param_send_lock(struct drm_connector* connector, int32_t param)
{
	ssize_t ret = -1;
	int32_t param_value;
	enum DISPPARAM_MODE io_cmd = (enum DISPPARAM_MODE)param;
	struct mtk_dsi *dsi = (struct mtk_dsi *)to_mtk_dsi(connector);
	struct mtk_ddp_comp *comp = NULL;
	//struct cmdq_pkt *cmdq_handle = NULL;
	if (!dsi) {
		pr_info("%s-%d:dsi is Null \n",__func__, __LINE__);
		return ret;
	}
	comp = &dsi->ddp_comp;
#if 0
	if (DISPPARAM_HBM_FOD_ON == param) {
		param_value = true;
		io_cmd = DSI_HBM_SET;
	}
	else if (DISPPARAM_HBM_FOD_OFF == param) {
		param_value = false;
		io_cmd = DSI_HBM_SET;
	}
	else if (DSI_SET_BL == param) {
		param_value = 20;
		io_cmd = DSI_SET_BL;
	}
#endif
	//cmdq_handle = cmdq_pkt_create(g_mtk_crtc->gce_obj.client[CLIENT_CFG]);
	ret = mtk_ddp_comp_io_cmd_dispparam(comp, NULL, io_cmd, (void *)&param_value);
	return ret;
}
static ssize_t mtk_dsi_host_send_cmd(struct mtk_dsi *dsi,
				     const struct mipi_dsi_msg *msg, u8 flag)
{
	mtk_dsi_wait_idle(dsi, flag, 2000, NULL);
	mtk_dsi_irq_data_clear(dsi, flag);
	mtk_dsi_cmdq(dsi, msg);
	mtk_dsi_start(dsi);

	if (MTK_DSI_HOST_IS_READ(msg->type)) {
		unsigned int loop_cnt = 0;
		s32 tmp;

		udelay(1);
		while (loop_cnt < 100 * 1000) {
			tmp = readl(dsi->regs + DSI_INTSTA);
			if ((tmp & LPRX_RD_RDY_INT_FLAG))
				break;
			loop_cnt++;
			usleep_range(100, 200);
		}
		DDPINFO("%s wait RXDY done\n", __func__);
		mtk_dsi_mask(dsi, DSI_INTSTA, LPRX_RD_RDY_INT_FLAG, 0);
		mtk_dsi_mask(dsi, DSI_RACK, RACK, RACK);
	}

	if (!mtk_dsi_wait_idle(dsi, flag, 2000, NULL))
		return -ETIME;
	else
		return 0;
}

static void mtk_dsi_dy_fps_cmdq_cb(struct cmdq_cb_data data)
{
	struct mtk_cmdq_cb_data *cb_data = data.data;

	DDPINFO("%s vdo mode fps change done\n", __func__);
	cmdq_pkt_destroy(cb_data->cmdq_handle);
	kfree(cb_data);
}

static ssize_t mtk_dsi_host_send_vm_cmd(struct mtk_dsi *dsi,
				     const struct mipi_dsi_msg *msg, u8 flag)
{
	unsigned int loop_cnt = 0;
	s32 tmp;

	mtk_dsi_vm_cmdq(dsi, msg, NULL);

	/* clear status */
	mtk_dsi_mask(dsi, DSI_INTSTA, VM_CMD_DONE_INT_EN, 0);
	mtk_dsi_vm_start(dsi);

	while (loop_cnt < 100 * 1000) {
		tmp = readl(dsi->regs + DSI_INTSTA);
		if (!(tmp & VM_CMD_DONE_INT_EN))
			return 0;
		loop_cnt++;
		udelay(1);
	}
	DDPMSG("%s timeout\n", __func__);
	return -ETIME;
}

static ssize_t mtk_dsi_host_transfer(struct mipi_dsi_host *host,
				     const struct mipi_dsi_msg *msg)
{
	struct mtk_dsi *dsi = host_to_dsi(host);
	u32 recv_cnt, i;
	u8 read_data[16];
	void *src_addr;
	u8 irq_flag;

	if (readl(dsi->regs + DSI_MODE_CTRL) & MODE)
		irq_flag = VM_CMD_DONE_INT_EN;
	else
		irq_flag = CMD_DONE_INT_FLAG;

	if (MTK_DSI_HOST_IS_READ(msg->type)) {
		struct mipi_dsi_msg set_rd_msg = {
		.tx_buf = (u8 [1]) { msg->rx_len},
		.tx_len = 0x1,
		.type = MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE,
		};

		if (mtk_dsi_host_send_cmd(dsi, &set_rd_msg, irq_flag) < 0)
			DDPPR_ERR("RX mtk_dsi_host_send_cmd fail\n");

		irq_flag |= LPRX_RD_RDY_INT_FLAG;
	}

	if (readl(dsi->regs + DSI_MODE_CTRL) & MODE) {
		if (mtk_dsi_host_send_vm_cmd(dsi, msg, irq_flag) < 0)
			return -ETIME;
	} else {
		if (mtk_dsi_host_send_cmd(dsi, msg, irq_flag) < 0)
			return -ETIME;
	}

	if (!MTK_DSI_HOST_IS_READ(msg->type))
		return 0;

	if (!msg->rx_buf) {
		DRM_ERROR("dsi receive buffer size may be NULL\n");
		return -EINVAL;
	}

	for (i = 0; i < 16; i++)
		*(read_data + i) = readb(dsi->regs + DSI_RX_DATA0 + i);

	recv_cnt = mtk_dsi_recv_cnt(read_data[0], read_data);

	if (recv_cnt > 2)
		src_addr = &read_data[4];
	else
		src_addr = &read_data[1];

	if (recv_cnt > 10)
		recv_cnt = 10;

	if (recv_cnt > msg->rx_len)
		recv_cnt = msg->rx_len;

	if (recv_cnt)
		memcpy(msg->rx_buf, src_addr, recv_cnt);

	DDPINFO("dsi get %d byte data from the panel address(0x%x)\n", recv_cnt,
		*((u8 *)(msg->tx_buf)));

	return recv_cnt;
}

static const struct mipi_dsi_host_ops mtk_dsi_ops = {
	.attach = mtk_dsi_host_attach,
	.detach = mtk_dsi_host_detach,
	.transfer = mtk_dsi_host_transfer,
};

void mtk_dsi_send_switch_cmd(struct mtk_dsi *dsi,
			struct cmdq_pkt *handle,
			struct mtk_drm_crtc *mtk_crtc, unsigned int cur_mode, unsigned int dst_mode)
{
	unsigned int i;
	struct dfps_switch_cmd *dfps_cmd = NULL;
	struct mtk_panel_params *params = NULL;
	struct drm_display_mode *old_mode = NULL;

	old_mode = &(mtk_crtc->avail_modes[cur_mode]);

	if (dsi->ext && dsi->ext->params)
		params = mtk_crtc->panel_ext->params;
	else /* can't find panel ext information,stop */
		return;

	for (i = 0; i < MAX_DYN_CMD_NUM; i++) {
		dfps_cmd = &params->dyn_fps.dfps_cmd_table[i];
		if (dfps_cmd->cmd_num == 0)
			break;

		if (dfps_cmd->src_fps == 0 || old_mode->vrefresh == dfps_cmd->src_fps)
			mipi_dsi_dcs_write_gce_dyn(dsi, handle, dfps_cmd->para_list,
				dfps_cmd->cmd_num);
	}
}

static void mtk_dsi_cmd_timing_change(struct mtk_dsi *dsi,
	struct mtk_drm_crtc *mtk_crtc, struct drm_crtc_state *old_state)
{
	struct cmdq_pkt *cmdq_handle;
	struct cmdq_pkt *cmdq_handle2;
	struct mtk_crtc_state *state =
	    to_mtk_crtc_state(mtk_crtc->base.state);
	struct mtk_crtc_state *old_mtk_state =
	    to_mtk_crtc_state(old_state);
	unsigned int src_mode =
	    old_mtk_state->prop_val[CRTC_PROP_DISP_MODE_IDX];
	unsigned int dst_mode =
	    state->prop_val[CRTC_PROP_DISP_MODE_IDX];
	bool need_mipi_change = 1;

	/* use no mipi clk change solution */
	if (dsi->ext && dsi->ext->params &&
		dsi->ext->params->dyn_fps.switch_en > 0)
		need_mipi_change = 0;

	mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
		mtk_crtc->gce_obj.client[CLIENT_CFG]);

	/* 1. wait frame done & wait DSI not busy */
	cmdq_pkt_wfe(cmdq_handle,
		mtk_crtc->gce_obj.event[EVENT_STREAM_EOF]);
	/* Clear stream block to prevent trigger loop start */
	cmdq_pkt_clear_event(cmdq_handle,
		mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
	mtk_dsi_poll_for_idle(dsi, cmdq_handle);
	cmdq_pkt_flush(cmdq_handle);
	cmdq_pkt_destroy(cmdq_handle);

	if (need_mipi_change == 0)
		goto skip_change_mipi;

	/*  send lcm cmd before DSI power down if needed */
	if (dsi->ext && dsi->ext->funcs &&
		dsi->ext->funcs->mode_switch)
		dsi->ext->funcs->mode_switch(dsi->panel, src_mode,
			dst_mode, BEFORE_DSI_POWERDOWN);

	/* Power off DSI */
	phy_power_off(dsi->phy);

	if (dsi->ext && dsi->ext->funcs &&
		dsi->ext->funcs->ext_param_set)
		dsi->ext->funcs->ext_param_set(dsi->panel,
			state->prop_val[CRTC_PROP_DISP_MODE_IDX]);

	/* Power on DSI */
	mtk_dsi_set_data_rate(dsi);
	phy_power_on(dsi->phy);
	mtk_dsi_phy_timconfig(dsi, NULL);
	//[FIXME] sw control enable will be set to 1 by mipi_tx_pll_prepare,
	//and it needs to clear to 0
	mtk_mipi_tx_sw_control_en(dsi->phy, 0);
	//[FIXME] It's a temp workaround for cmd mode.
	writel(0x0001023c, dsi->regs + DSI_TXRX_CTRL);

skip_change_mipi:
	/*  send lcm cmd after DSI power on if needed */
	if (dsi->ext && dsi->ext->funcs &&
		dsi->ext->funcs->mode_switch)
		dsi->ext->funcs->mode_switch(dsi->panel, src_mode,
			dst_mode, AFTER_DSI_POWERON);

	/* set frame done */
	mtk_crtc_pkt_create(&cmdq_handle2, &mtk_crtc->base,
		mtk_crtc->gce_obj.client[CLIENT_CFG]);
	cmdq_pkt_set_event(cmdq_handle2,
		mtk_crtc->gce_obj.event[EVENT_STREAM_EOF]);
	cmdq_pkt_set_event(cmdq_handle2,
		mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
	cmdq_pkt_flush(cmdq_handle2);
	cmdq_pkt_destroy(cmdq_handle2);
}

static void mtk_dsi_vdo_timing_change(struct mtk_dsi *dsi,
	struct mtk_drm_crtc *mtk_crtc, struct drm_crtc_state *old_state)
{
	unsigned int vfp = 0;
	unsigned int hfp = 0;
	unsigned int fps_chg_index = 0;
	struct cmdq_pkt *handle;
	struct cmdq_client *client = mtk_crtc->gce_obj.client[CLIENT_DSI_CFG];
	struct mtk_ddp_comp *comp = &dsi->ddp_comp;
	struct mtk_crtc_state *state =
	    to_mtk_crtc_state(mtk_crtc->base.state);
	struct mtk_cmdq_cb_data *cb_data;
	struct drm_display_mode adjusted_mode = state->base.adjusted_mode;
	struct mtk_crtc_state *old_mtk_state =
			to_mtk_crtc_state(old_state);
	unsigned int src_mode =
	    old_mtk_state->prop_val[CRTC_PROP_DISP_MODE_IDX];

	DDPINFO("%s+\n", __func__);

	if (dsi->ext && dsi->ext->funcs &&
		dsi->ext->funcs->ext_param_set)
		dsi->ext->funcs->ext_param_set(dsi->panel,
			state->prop_val[CRTC_PROP_DISP_MODE_IDX]);
	//1.fps change index
	fps_chg_index = mtk_crtc->fps_change_index;

	mtk_drm_idlemgr_kick(__func__, &(mtk_crtc->base), 0);

	cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
	if (!cb_data) {
		DDPINFO("%s:%d, cb data creation failed\n",
				__func__, __LINE__);
		return;
	}
	mtk_crtc_pkt_create(&handle, &(mtk_crtc->base), client);

	if (fps_chg_index & DYNFPS_DSI_MIPI_CLK) {
		DDPINFO("%s, change MIPI Clock\n", __func__);
	} else if (fps_chg_index & DYNFPS_DSI_HFP) {
		DDPINFO("%s, change HFP\n", __func__);
		/*wait and clear EOF
		 * avoid other display related task break fps change task
		 * because fps change need stop & re-start vdo mode
		 */
		cmdq_pkt_wfe(handle,
			     mtk_crtc->gce_obj.event[EVENT_VDO_EOF]);
		/*1.1 send cmd: stop vdo mode*/
		mtk_dsi_stop_vdo_mode(dsi, handle);
		/* for crtc first enable,dyn fps fail*/
		if (dsi->data_rate == 0) {
			dsi->data_rate = mtk_dsi_default_rate(dsi);
			mtk_mipi_tx_pll_rate_set_adpt(dsi->phy, dsi->data_rate);

			mtk_dsi_phy_timconfig(dsi, NULL);
		}
		if (dsi->mipi_hopping_sta) {
			DDPINFO("%s,mipi_clk_change_sta\n", __func__);
			hfp = dsi->ext->params->dyn.hfp;
		} else
			hfp = adjusted_mode.hsync_start -
				adjusted_mode.hdisplay;
		dsi->vm.hfront_porch = hfp;

		mtk_dsi_calc_vdo_timing(dsi);
		mtk_dsi_porch_setting(comp, handle, DSI_HFP, dsi->hfp_byte);

		/*1.2 send cmd: send cmd*/
		mtk_dsi_send_switch_cmd(dsi, handle, mtk_crtc, src_mode, adjusted_mode.vrefresh);
		/*1.3 send cmd: start vdo mode*/
		mtk_dsi_start_vdo_mode(comp, handle);
		/*clear EOF
		 * avoid config continue after we trigger vdo mode
		 */
		cmdq_pkt_clear_event(handle,
			     mtk_crtc->gce_obj.event[EVENT_VDO_EOF]);
		/*1.3 send cmd: trigger*/
		mtk_disp_mutex_trigger(comp->mtk_crtc->mutex[0], handle);
		mtk_dsi_trigger(comp, handle);
	} else if (fps_chg_index & DYNFPS_DSI_VFP) {
		DDPINFO("%s, change VFP\n", __func__);

		cmdq_pkt_clear_event(handle,
				mtk_crtc->gce_obj.event[EVENT_DSI0_SOF]);

		cmdq_pkt_wait_no_clear(handle,
			mtk_crtc->gce_obj.event[EVENT_DSI0_SOF]);
			comp = mtk_ddp_comp_request_output(mtk_crtc);

			if (!comp) {
				DDPPR_ERR("ddp comp is NULL\n");
				return;
			}

			if (dsi->mipi_hopping_sta) {
				DDPINFO("%s,mipi_clk_change_sta\n", __func__);
				vfp = dsi->ext->params->dyn.vfp;
			} else
				vfp = adjusted_mode.vsync_start -
					adjusted_mode.vdisplay;
			dsi->vm.vfront_porch = vfp;

		if (dsi->mipi_hopping_sta) {
			DDPINFO("%s,mipi_clk_change_sta\n", __func__);
			vfp = dsi->ext->params->dyn.vfp;
		} else
			vfp = adjusted_mode.vsync_start -
				adjusted_mode.vdisplay;
		dsi->vm.vfront_porch = vfp;

		mtk_dsi_porch_setting(comp, handle, DSI_VFP, vfp);
	}
	cb_data->cmdq_handle = handle;
	if (cmdq_pkt_flush_threaded(handle,
		mtk_dsi_dy_fps_cmdq_cb, cb_data) < 0)
		DDPPR_ERR("failed to flush dsi_dy_fps\n");
}

static void mtk_dsi_timing_change(struct mtk_dsi *dsi,
	struct mtk_drm_crtc *mtk_crtc, struct drm_crtc_state *old_state)
{
	if (mtk_dsi_is_cmd_mode(&dsi->ddp_comp))
		mtk_dsi_cmd_timing_change(dsi, mtk_crtc, old_state);
	else
		mtk_dsi_vdo_timing_change(dsi, mtk_crtc, old_state);
}

int mtk_show_brightness_clone(struct mtk_ddp_comp *comp)
{
	struct mtk_dsi *dsi;
	int brightness = 0;
	dsi = container_of(comp, struct mtk_dsi, ddp_comp);

	if(dsi->conn.brightness_clone) {
		brightness = dsi->conn.brightness_clone;
	}

	return brightness;
}

static int mtk_dsi_set_dynamic_fps(struct mtk_dsi *dsi,
		unsigned int cur_mode, unsigned int dst_mode)
{
	int ret = 0;

	if(dsi->ext->funcs->panel_freq_switch) {
		ret = dsi->ext->funcs->panel_freq_switch(dsi->panel, cur_mode, dst_mode);
	}
	if(dsi->ext->funcs->ext_param_set) {
		ret = dsi->ext->funcs->ext_param_set(dsi->panel, 1);
	}
	return ret;
}

unsigned int mtk_dsi_get_dsc_compress_rate(struct mtk_dsi *dsi)
{
	unsigned int compress_rate, bpp, bpc;
	struct mtk_panel_ext *ext = dsi->ext;

	if (ext->params->dsc_params.enable) {
		bpp = ext->params->dsc_params.bit_per_pixel / 16;
		bpc = ext->params->dsc_params.bit_per_channel;
		//compress_rate*100 for 3.75 or 2.5 case
		compress_rate = bpc * 3 * 100 / bpp;
	} else
		compress_rate = 100;

	return compress_rate;
}

/******************************************************************************
 * HRT BW = Overlap x vact x hact x vrefresh x 4 x (vtotal/vact)
 * In Video Mode , Using the Formula below:
 * MM Clock
 * DSC on:  vact x hact x vrefresh x  (vtotal / vact)
 * DSC off: vact x hact x vrefresh x (vtotal x htotal) / (vact x hact)

 * In Command Mode Using the Formula below:
 * Type     | MM Clock (unit: Pixel)
 * CPHY     | data_rate x (16/7) x lane_num x compress_ratio / bpp
 * DPHY     | data_rate x lane_num x compress_ratio / bpp
 ******************************************************************************/
void mtk_dsi_set_mmclk_by_datarate(struct mtk_dsi *dsi,
	struct mtk_drm_crtc *mtk_crtc, unsigned int en)
{
	struct mtk_panel_ext *ext = dsi->ext;
	unsigned int compress_rate;
	unsigned int data_rate;
	unsigned int pixclk = 0;
	u32 bpp = mipi_dsi_pixel_format_to_bpp(dsi->format);
	unsigned int pixclk_min = 0;
	unsigned int hact = mtk_crtc->base.state->adjusted_mode.hdisplay;
	unsigned int htotal = mtk_crtc->base.state->adjusted_mode.htotal;
	unsigned int vtotal = mtk_crtc->base.state->adjusted_mode.vtotal;
	unsigned int vact = mtk_crtc->base.state->adjusted_mode.vdisplay;
	unsigned int vrefresh = mtk_crtc->base.state->adjusted_mode.vrefresh;

	if (!en) {
		mtk_drm_set_mmclk_by_pixclk(&mtk_crtc->base, pixclk,
					__func__);
		return;
	}
	//for FPS change,update dsi->ext
	dsi->ext = find_panel_ext(dsi->panel);
	data_rate = mtk_dsi_default_rate(dsi);

	if (!dsi->ext) {
		DDPPR_ERR("DSI panel ext is NULL\n");
		return;
	}

	compress_rate = mtk_dsi_get_dsc_compress_rate(dsi);

	if (!data_rate) {
		DDPPR_ERR("DSI data_rate is NULL\n");
		return;
	}
	//If DSI mode is vdo mode
	if (!mtk_dsi_is_cmd_mode(&dsi->ddp_comp)) {
		if (ext->params->is_cphy)
			pixclk_min = data_rate * dsi->lanes * 2 / 7 / 3;
		else
			pixclk_min = data_rate * dsi->lanes / 8 / 3;

		pixclk = vact * hact * vrefresh / 1000;

		if (ext->params->dsc_params.enable)
			pixclk = pixclk * vtotal / vact;
		else
			pixclk = pixclk * (vtotal * htotal * 100 /
				(vact * hact)) / 100;
		pixclk = (unsigned int)(pixclk / 1000);
		pixclk = (pixclk_min > pixclk) ? pixclk_min : pixclk;
	}

	else {
		pixclk = data_rate * dsi->lanes * compress_rate;
		if (data_rate && ext->params->is_cphy)
			pixclk = pixclk * 16 / 7;
		pixclk = pixclk / bpp / 100;
	}
	DDPINFO("%s,data_rate =%d,clk=%u pixclk_min=%d\n", __func__,
			data_rate, pixclk, pixclk_min);
	mtk_drm_set_mmclk_by_pixclk(&mtk_crtc->base, pixclk, __func__);
}

/******************************************************************************
 * DSI Type | PHY TYPE | HRT_BW (unit: Bytes) one frame ( Overlap * )
 * VDO MODE | CPHY/DPHY| Overlap x vact x hact x vrefresh x 4 x (vtotal/vact)
 * CMD MODE | CPHY     | (16/7) x data_rate x lane_num x compress_ratio/ bpp x4
 * CMD MODE | DPHY     | data_rate x lane_num x compress_ratio / bpp x 4
 ******************************************************************************/
unsigned long long mtk_dsi_get_frame_hrt_bw_base_by_datarate(
		struct mtk_drm_crtc *mtk_crtc,
		struct mtk_dsi *dsi)
{
	static unsigned long long bw_base;
	int hact = mtk_crtc->base.state->adjusted_mode.hdisplay;
	int vtotal = mtk_crtc->base.state->adjusted_mode.vtotal;
	int vact = mtk_crtc->base.state->adjusted_mode.vdisplay;
	int vrefresh = mtk_crtc->base.state->adjusted_mode.vrefresh;

	//For CMD mode to calculate HRT BW
	unsigned int compress_rate = mtk_dsi_get_dsc_compress_rate(dsi);
	unsigned int data_rate = mtk_dsi_default_rate(dsi);
	u32 bpp = mipi_dsi_pixel_format_to_bpp(dsi->format);

	bw_base = vact * hact * vrefresh * 4 / 1000;
	if (!mtk_dsi_is_cmd_mode(&dsi->ddp_comp)) {
		bw_base = bw_base * vtotal / vact;
		bw_base = bw_base / 1000;
	} else {
		bw_base = data_rate * dsi->lanes * compress_rate * 4;
		bw_base = bw_base / bpp / 100;
	}
	DDPDBG("Frame Bw:%llu",	bw_base);
	return bw_base;
}
static int mtk_dsi_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum mtk_ddp_io_cmd cmd, void *params)
{
	struct mtk_panel_ext **ext;
	struct mtk_dsi *dsi = container_of(comp, struct mtk_dsi, ddp_comp);
	void **out_params;
	struct mtk_panel_ext *panel_ext = NULL;
	struct drm_display_mode **mode;
	bool *enable;
	unsigned int vfp_low_power = 0;
	pr_debug("%s-%d:dsi = %p, cmd = %s \n",__func__, __LINE__, dsi, get_cmd_name(cmd));

	switch (cmd) {
	case REQ_PANEL_EXT:
		ext = (struct mtk_panel_ext **)params;

		*ext = mtk_dsi_get_panel_ext(comp);
		break;
	case DSI_START_VDO_MODE:
		mtk_dsi_start_vdo_mode(comp, handle);
		break;
	case DSI_STOP_VDO_MODE:
		mtk_dsi_stop_vdo_mode(dsi, handle);
		break;
	case ESD_CHECK_READ:
		mtk_dsi_esd_read(comp, handle, (uintptr_t)params);
		break;
	case ESD_CHECK_CMP:
		return mtk_dsi_esd_cmp(comp, handle, params);
	case CONNECTOR_READ_EPILOG:
		mtk_dsi_clear_rxrd_irq(dsi);
		break;
	case REQ_ESD_EINT_COMPAT:
		out_params = (void **)params;

		*out_params = (void *)dsi->driver_data->esd_eint_compat;
		break;
	case COMP_REG_START:
		mtk_dsi_trigger(comp, handle);
		break;
	case CONNECTOR_PANEL_ENABLE:
		mtk_output_dsi_enable(dsi, true);
		break;
	case CONNECTOR_PANEL_DISABLE:
	{
		mtk_output_dsi_disable(dsi, true);
		dsi->doze_enabled = false;
	}
		break;
	case CONNECTOR_ENABLE:
		mtk_dsi_leave_idle(dsi);
		break;
	case CONNECTOR_DISABLE:
		mtk_dsi_enter_idle(dsi);
		break;
	case CONNECTOR_RESET:
		mtk_dsi_reset_engine(dsi);
		break;
	case CONNECTOR_IS_ENABLE:
		enable = (bool *)params;
		*enable = dsi->output_en;
		break;
	case DSI_VFP_IDLE_MODE:
	{
		panel_ext = mtk_dsi_get_panel_ext(comp);

		if (dsi->mipi_hopping_sta && panel_ext && panel_ext->params
			&& panel_ext->params->dyn.vfp_lp_dyn)
			vfp_low_power = panel_ext->params->dyn.vfp_lp_dyn;
		else if (panel_ext && panel_ext->params
			&& panel_ext->params->vfp_low_power)
			vfp_low_power = panel_ext->params->vfp_low_power;
		if (vfp_low_power) {
			DDPINFO("vfp_low_power=%d\n", vfp_low_power);
			mtk_dsi_porch_setting(comp, handle, DSI_VFP,
					vfp_low_power);
		}
	}
		break;
	case DSI_VFP_DEFAULT_MODE:
	{
		unsigned int vfront_porch = 0;
		struct mtk_drm_crtc *crtc = comp->mtk_crtc;
		panel_ext = mtk_dsi_get_panel_ext(comp);

		if (dsi->mipi_hopping_sta && panel_ext && panel_ext->params
			&& panel_ext->params->dyn.vfp)
			vfront_porch = panel_ext->params->dyn.vfp;
		else
			vfront_porch = dsi->vm.vfront_porch;

		DDPINFO("vfront_porch=%d\n", vfront_porch);

		if (panel_ext->params->wait_sof_before_dec_vfp) {
			cmdq_pkt_clear_event(handle,
				crtc->gce_obj.event[EVENT_DSI0_SOF]);
			cmdq_pkt_wait_no_clear(handle,
				crtc->gce_obj.event[EVENT_DSI0_SOF]);
		}
		mtk_dsi_porch_setting(comp, handle, DSI_VFP,
					vfront_porch);
	}
		break;
	case DSI_GET_TIMING:
		mode = (struct drm_display_mode **)params;
		*mode = list_first_entry(&dsi->conn.modes,
			struct drm_display_mode, head);
		break;

	case DSI_GET_MODE_BY_MAX_VREFRESH:
	{
		struct drm_display_mode *tmp = NULL;
		unsigned int vrefresh = 0;

		mode = (struct drm_display_mode **)params;
		list_for_each_entry(tmp, &dsi->conn.modes, head) {
			if (tmp && tmp->vrefresh > vrefresh) {
				vrefresh = tmp->vrefresh;
				*mode = tmp;
			}
		}
	}
		break;

	case IRQ_LEVEL_IDLE:
	{
		unsigned int inten;

		if (!mtk_dsi_is_cmd_mode(&dsi->ddp_comp) && handle) {
			inten = FRAME_DONE_INT_FLAG;
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DSI_INTEN, 0, inten);
		}
	}
		break;
	case IRQ_LEVEL_ALL:
	{
		unsigned int inten;

		if (!handle) {
			DDPPR_ERR("GCE handle is NULL\n");
			return 0;
		}

		inten = BUFFER_UNDERRUN_INT_FLAG | INP_UNFINISH_INT_EN;

		if (!mtk_dsi_is_cmd_mode(&dsi->ddp_comp)) {
			inten |= FRAME_DONE_INT_FLAG;
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DSI_INTEN, inten, inten);
		} else {
			inten |= TE_RDY_INT_FLAG;
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DSI_INTEN, inten, inten);
		}
	}
		break;
	case LCM_RESET:
	{
		struct mtk_dsi *dsi =
			container_of(comp, struct mtk_dsi, ddp_comp);

		panel_ext = mtk_dsi_get_panel_ext(comp);
		if (panel_ext && panel_ext->funcs
			&& panel_ext->funcs->reset)
			panel_ext->funcs->reset(dsi->panel, *(int *)params);
	}
		break;
	case DSI_SET_BL:
	{
		struct mtk_dsi *dsi =
			container_of(comp, struct mtk_dsi, ddp_comp);


		panel_ext = mtk_dsi_get_panel_ext(comp);
		if (panel_ext && panel_ext->funcs
			&& panel_ext->funcs->set_backlight_cmdq)
			panel_ext->funcs->set_backlight_cmdq(dsi,
					mipi_dsi_dcs_write_gce,
					handle, *(int *)params);
	}
		break;
	case DSI_SET_BL_AOD:
	{
		struct mtk_dsi *dsi =
			container_of(comp, struct mtk_dsi, ddp_comp);

		panel_ext = mtk_dsi_get_panel_ext(comp);
		if (panel_ext && panel_ext->funcs
			&& panel_ext->funcs->set_aod_light_mode)
			panel_ext->funcs->set_aod_light_mode(dsi->panel, dsi,
					mipi_dsi_dcs_write_gce,
					handle, *(unsigned int *)params);
	}
		break;

	case DSI_SET_BL_GRP:
	{
		struct mtk_dsi *dsi =
			container_of(comp, struct mtk_dsi, ddp_comp);


		panel_ext = mtk_dsi_get_panel_ext(comp);
		if (panel_ext && panel_ext->funcs
			&& panel_ext->funcs->set_backlight_grp_cmdq)
			panel_ext->funcs->set_backlight_grp_cmdq(dsi,
					mipi_dsi_dcs_grp_write_gce,
					handle, *(int *)params);
	}
		break;
	case DSI_HBM_SET:
	{
		panel_ext = mtk_dsi_get_panel_ext(comp);
		if (*(bool *)params) {
			pr_info("fod_backlight_flag on\n");
			dsi->fod_backlight_flag = true;
			dsi->fod_hbm_flag = true;
		} else {
			pr_info("fod_backlight_flag off\n");
			dsi->fod_backlight_flag = false;
			dsi->fod_hbm_flag = false;
			if (dsi->normal_hbm_flag) {
				if (!(panel_ext && panel_ext->funcs &&
					panel_ext->funcs->hbm_set_state)) {
					break;
				}
				panel_ext->funcs->hbm_set_state(dsi->panel, &dsi->fod_hbm_flag);
				break;
			}
		}

		if (!(panel_ext && panel_ext->funcs &&
			panel_ext->funcs->hbm_set_cmdq)) {
				break;
		}

		panel_ext->funcs->hbm_set_cmdq(dsi->panel, dsi,
					       mipi_dsi_dcs_write_gce, handle,
					       *(bool *)params);
		break;
	}
	case DSI_HBM_GET_STATE:
	{
		panel_ext = mtk_dsi_get_panel_ext(comp);
		if (!(panel_ext && panel_ext->funcs &&
		      panel_ext->funcs->hbm_get_state))
			break;

		panel_ext->funcs->hbm_get_state(dsi->panel, (bool *)params);
		break;
	}
	case DSI_HBM_GET_WAIT_STATE:
	{
		panel_ext = mtk_dsi_get_panel_ext(comp);
		if (!(panel_ext && panel_ext->funcs &&
		      panel_ext->funcs->hbm_get_wait_state))
			break;

		panel_ext->funcs->hbm_get_wait_state(dsi->panel,
						     (bool *)params);
		break;
	}
	case DSI_HBM_SET_WAIT_STATE:
	{
		panel_ext = mtk_dsi_get_panel_ext(comp);
		if (!(panel_ext && panel_ext->funcs &&
		      panel_ext->funcs->hbm_set_wait_state))
			break;

		panel_ext->funcs->hbm_set_wait_state(dsi->panel,
						     *(bool *)params);
		break;
	}
	case DSI_HBM_WAIT:
	{
		int ret = 0;

		if (mtk_dsi_is_cmd_mode(&dsi->ddp_comp)) {
			reset_dsi_wq(&dsi->te_rdy);
			ret = wait_dsi_wq(&dsi->te_rdy, HZ);
		} else {
			reset_dsi_wq(&dsi->frame_done);
			ret = wait_dsi_wq(&dsi->frame_done, HZ);
		}
		if (!ret)
			DDPINFO("%s: DSI_HBM_WAIT failed\n", __func__);
		break;
	}
	case LCM_ATA_CHECK:
	{
		struct mtk_dsi *dsi =
			container_of(comp, struct mtk_dsi, ddp_comp);
		int *val = (int *)params;

		panel_ext = mtk_dsi_get_panel_ext(comp);
		if (panel_ext && panel_ext->funcs
			&& panel_ext->funcs->ata_check)
			*val = panel_ext->funcs->ata_check(dsi->panel);
	}
		break;
	case DSI_SET_CRTC_AVAIL_MODES:
	{
		struct mtk_drm_crtc *crtc = (struct mtk_drm_crtc *)params;
		struct drm_display_mode *m;
		unsigned int i = 0;

		crtc->avail_modes_num = 0;
		list_for_each_entry(m, &dsi->conn.modes, head)
			crtc->avail_modes_num++;

		crtc->avail_modes =
		    vzalloc(sizeof(struct drm_display_mode) *
			    crtc->avail_modes_num);
		list_for_each_entry(m, &dsi->conn.modes, head) {
			drm_mode_copy(&crtc->avail_modes[i], m);
			i++;
		}
	}
		break;
	case DSI_TIMING_CHANGE:
	{
		struct mtk_drm_crtc *crtc = comp->mtk_crtc;
		struct drm_crtc_state *old_state =
		    (struct drm_crtc_state *)params;

		mtk_dsi_timing_change(dsi, crtc, old_state);
	}
		break;
	case DSI_SET_IDLE_FPS:
	{
		unsigned int *src_mode = (unsigned int *)params;

		if(*src_mode != 0) {
			mtk_dsi_set_dynamic_fps(dsi, *src_mode, 0);
		}
	}
		break;
	case DSI_SET_NON_IDLE_FPS:
	{
		unsigned int *src_mode = (unsigned int *)params;

		if(*src_mode != 0) {
		mtk_dsi_set_dynamic_fps(dsi, 0, *src_mode);
		}
	}
		break;
	case GET_PANEL_NAME:
	{
		struct mtk_dsi *dsi =
			container_of(comp, struct mtk_dsi, ddp_comp);
		out_params = (void **)params;

		*out_params = (void *)dsi->panel->dev->driver->name;
	}
		break;
	case DSI_CHANGE_MODE:
	{
		struct mtk_dsi *dsi =
			container_of(comp, struct mtk_dsi, ddp_comp);
		int *aod_en = params;

		panel_ext = mtk_dsi_get_panel_ext(comp);
		if (dsi->ext && dsi->ext->funcs
			&& dsi->ext->funcs->doze_get_mode_flags) {

			dsi->mode_flags =
				dsi->ext->funcs->doze_get_mode_flags(
					dsi->panel, *aod_en);
		}
	}
		break;
	case MIPI_HOPPING:
	{
		struct mtk_dsi *dsi =
			container_of(comp, struct mtk_dsi, ddp_comp);
		int *en = (int *)params;

		mtk_dsi_clk_change(dsi, *en);
	}
		break;
	case DYN_FPS_INDEX:
	{
		struct mtk_drm_crtc *crtc = comp->mtk_crtc;
		struct drm_crtc_state *old_state =
		    (struct drm_crtc_state *)params;
		mtk_dsi_fps_change_index(dsi, crtc, old_state);
	}
		break;
	case SET_MMCLK_BY_DATARATE:
	{
#ifndef CONFIG_FPGA_EARLY_PORTING

		struct mtk_drm_crtc *crtc = comp->mtk_crtc;
		unsigned int *pixclk = (unsigned int *)params;

		mtk_dsi_set_mmclk_by_datarate(dsi, crtc, *pixclk);
#endif
	}
		break;
	case GET_FRAME_HRT_BW_BY_DATARATE:
	{
		struct mtk_drm_crtc *crtc = comp->mtk_crtc;
		unsigned long long *base_bw =
			(unsigned long long *)params;

		*base_bw = mtk_dsi_get_frame_hrt_bw_base_by_datarate(crtc, dsi);
	}
		break;
	case DSI_SEND_DDIC_CMD:
	{
		struct mtk_drm_crtc *crtc = comp->mtk_crtc;
		struct mtk_ddic_dsi_msg *cmd_msg =
			(struct mtk_ddic_dsi_msg *)params;

		return mtk_mipi_dsi_write_gce(dsi, handle, crtc, cmd_msg);
	}
		break;
	case DSI_READ_DDIC_CMD:
	{
		struct mtk_drm_crtc *crtc = comp->mtk_crtc;
		struct mtk_ddic_dsi_msg *cmd_msg =
			(struct mtk_ddic_dsi_msg *)params;

		return mtk_mipi_dsi_read_gce(dsi, handle, crtc, cmd_msg);
	}
		break;
	case DSI_GET_VIRTUAL_HEIGH:
	{
		struct mtk_drm_crtc *crtc = comp->mtk_crtc;

		return mtk_dsi_get_virtual_heigh(dsi, &crtc->base);
	}
		break;
	case DSI_GET_VIRTUAL_WIDTH:
	{
		struct mtk_drm_crtc *crtc = comp->mtk_crtc;

		return mtk_dsi_get_virtual_width(dsi, &crtc->base);
	}
		break;
	case DSI_HBM_NEED_DELAY:
	{
		panel_ext = mtk_dsi_get_panel_ext(comp);
		if (!(panel_ext && panel_ext->funcs &&
		      panel_ext->funcs->hbm_need_delay)) {
			*((bool *)params) = false;
			break;
		}

		panel_ext->funcs->hbm_need_delay(dsi->panel, (bool *)params);
	}
		break;
	case DSI_LFR_SET:
	{
		mtk_dsi_set_LFR(dsi, comp, handle);
	}
		break;
	case DSI_LFR_UPDATE:
	{
		mtk_dsi_LFR_update(dsi, comp, handle);
	}
		break;
	case DSI_LFR_STATUS_CHECK:
	{
		mtk_dsi_LFR_status_check(dsi);
	}
		break;
	case DSI_HBM_SOLUTION:
	{
		panel_ext = mtk_dsi_get_panel_ext(comp);
		if (!(panel_ext && panel_ext->funcs &&
		      panel_ext->funcs->get_hbm_solution))
			break;

		panel_ext->funcs->get_hbm_solution(dsi->panel, (int *)params);
	}
		break;

	default:
		break;
	}
	return 0;
}

#ifdef CONFIG_DRM_DFPS
extern int mtkfb_enable_idlemgr_dfps(bool en);
#endif

static int mtk_dsi_io_cmd_dispparam(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum DISPPARAM_MODE cmd, void *params)
{
	struct mtk_dsi *dsi = container_of(comp, struct mtk_dsi, ddp_comp);
	struct mtk_panel_ext *panel_ext = NULL;
	enum DISPPARAM_MODE cmd_temp;
	u32 fod_backlight = 0;
	static u32 backlight_by_brightness = 0x40;
	int  doze_state;
	static bool in_aod = false;
	int unset_doze_brightness;
	static int panel_power_mode = DRM_BLANK_UNBLANK;
	bool fod_backlight_flag;
	bool hbm_en;
#ifdef CONFIG_DRM_DFPS
	bool dfps_en;
#endif
	bool need_dsi_lock = true;
	uint32_t is_thermal_call = 0x1;

	cmd_temp = cmd & 0xF0000000;
	if (DISPPARAM_RESTORE_BACKLIGHT == cmd_temp)
		need_dsi_lock = false;

	pr_info("%s-%d:dsi = %p, cmd = %s \n",__func__, __LINE__, dsi, get_dispparam_name(cmd));

	if (need_dsi_lock) {
		mutex_lock(&dsi->dsi_lock);
		pr_info("%s mutex lock\n", __func__);
	} else {
		pr_info("%s esd recovery not need mutex lock\n", __func__);
	}

	cmd_temp = cmd & 0x0F00;
	switch (cmd_temp) {
		case DISPPARAM_DIMMING_OFF:
		{
			panel_ext = mtk_dsi_get_panel_ext(comp);
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->panel_dimming_control))
				break;
			panel_ext->funcs->panel_dimming_control(dsi->panel, false);
			break;
		}

		case DISPPARAM_DIMMING:
		{
			panel_ext = mtk_dsi_get_panel_ext(comp);
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->panel_dimming_control))
				break;
			panel_ext->funcs->panel_dimming_control(dsi->panel, true);
			break;
		}
		default:
			break;
	}

	cmd_temp = cmd & 0x0F0000;
	switch (cmd_temp) {
		case DISPPARAM_DC_ON:
		{
			dsi->dc_flag = true;
			pr_info("dc_status on\n");
			break;
		}

		case DISPPARAM_DC_OFF:
		{
			dsi->dc_flag = false;
			pr_info("dc_status off\n");
			break;
		}
		case DISPPARAM_HBM_ON:
		{
			panel_ext = mtk_dsi_get_panel_ext(comp);
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->normal_hbm_control))
				break;

			is_thermal_call = *((uint32_t*)params);

			pr_info("%s, hbm:%x, thermal_call:%x\n", __func__, dsi->fod_hbm_flag, is_thermal_call);
			if(!dsi->fod_hbm_flag && !dsi->thermal_hbm_disabled)
				panel_ext->funcs->normal_hbm_control(dsi->panel, true);
#ifdef CONFIG_HWCONF_MANAGER
			dsi_panel_HBM_count(dsi->panel, 1, 0);
#endif
			if (is_thermal_call == 0x1) {
				pr_info("thermal clear hbm limit, restore previous hbm on\n");
			} else {
				dsi->normal_hbm_flag = true;
			}

			break;
		}
		case DISPPARAM_HBM_FOD_ON:
		{
			panel_ext = mtk_dsi_get_panel_ext(comp);
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->hbm_fod_control))
				break;
			pr_info("fod_backlight_flag on\n");
			dsi->fod_backlight_flag = true;
			dsi->fod_hbm_flag = true;
			panel_ext->funcs->hbm_fod_control(dsi->panel, true);
			break;
		}
		case DISPPARAM_HBM_OFF:
		{
			is_thermal_call = *((uint32_t*)params);

			pr_info("%s, thermal_call:%x, hbm:%x\n", __func__, is_thermal_call, dsi->normal_hbm_flag);
			if (is_thermal_call == 0x1) {
				pr_info("thermal set hbm limit, hbm off\n");
			} else {
				dsi->normal_hbm_flag = false;
			}

			if (dsi->fod_hbm_flag)
				break;
			panel_ext = mtk_dsi_get_panel_ext(comp);
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->normal_hbm_control))
				break;
			panel_ext->funcs->normal_hbm_control(dsi->panel, false);
#ifdef CONFIG_HWCONF_MANAGER
			dsi_panel_HBM_count(dsi->panel, 0, 0);
#endif
			break;
		}
		case DISPPARAM_HBM_FOD_OFF:
		{
			pr_info("fod_backlight_flag off\n");
			dsi->fod_backlight_flag = false;
			dsi->fod_hbm_flag = false;
			panel_ext = mtk_dsi_get_panel_ext(comp);
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->hbm_fod_control))
				break;

			panel_ext->funcs->hbm_fod_control(dsi->panel, false);
			break;
		}
		case DISPPARAM_DOZE_BRIGHTNESS_HBM:
		{
			panel_ext = mtk_dsi_get_panel_ext(comp);
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->set_doze_brightness))
				break;
			pr_info("set DISPPARAM_DOZE_BRIGHTNESS_HBM\n");
			panel_ext->funcs->set_doze_brightness(dsi->panel, DOZE_BRIGHTNESS_HBM);
			break;
		}
		case DISPPARAM_DOZE_BRIGHTNESS_LBM:
		{
			panel_ext = mtk_dsi_get_panel_ext(comp);
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->set_doze_brightness))
				break;
			pr_info("set DISPPARAM_DOZE_BRIGHTNESS_LBM\n");
			panel_ext->funcs->set_doze_brightness(dsi->panel, DOZE_BRIGHTNESS_LBM);
			break;
		}

		default:
			break;
	}

	cmd_temp = cmd & 0x0F00000;
	switch (cmd_temp) {
		case DISPPARAM_SRGB:
		{
			panel_ext = mtk_dsi_get_panel_ext(comp);
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->panel_set_crc_srgb))
				break;
			pr_info("CRC srgb");
			panel_ext->funcs->panel_set_crc_srgb(dsi->panel);
			break;
		}

		case DISPPARAM_P3:
		{
			panel_ext = mtk_dsi_get_panel_ext(comp);
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->panel_set_crc_p3))
				break;
			pr_info("CRC p3");
			panel_ext->funcs->panel_set_crc_p3(dsi->panel);
			break;
		}

		case DISPPARAM_CRC_P3_D65:
		{
			panel_ext = mtk_dsi_get_panel_ext(comp);
			if (!(panel_ext && panel_ext->funcs &&
				  panel_ext->funcs->panel_set_crc_p3_d65))
				break;
			pr_info("CRC p3 d65");
			panel_ext->funcs->panel_set_crc_p3_d65(dsi->panel);
			break;
		}

		case DISPPARAM_CRC_OFF:
		{
			panel_ext = mtk_dsi_get_panel_ext(comp);
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->panel_set_crc_off))
				break;
			pr_info("CRC off");
			panel_ext->funcs->panel_set_crc_off(dsi->panel);

			break;
		}
		case DISPPARAM_FOD_BACKLIGHT:
		{
			fod_backlight = cmd & 0x1FFF;
			panel_ext = mtk_dsi_get_panel_ext(comp);
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->setbacklight_control))
				break;
			if (0x1000 == fod_backlight) {
				*(u32 *)params = backlight_by_brightness;
			}
			else {
				*(u32 *)params = (fod_backlight & 0x7ff) ;
			}
			pr_info("fod backlight = 0x%x \n", *(u32 *)params);
			panel_ext->funcs->setbacklight_control(dsi->panel, *(u32 *)params, true);

			break;
		}
		default:
			break;
	}

	cmd_temp = cmd & 0x0F000000;
	switch (cmd_temp) {
		case DISPPARAM_FOD_BACKLIGHT_ON:
		{
			pr_info("fod_backlight_flag on\n");
			dsi->fod_backlight_flag = true;
			break;
		}

		case DISPPARAM_FOD_BACKLIGHT_OFF:
		{
			pr_info("fod_backlight_flag false\n");
			dsi->fod_backlight_flag = false;
			break;
		}

		case DISPPARAM_FLAT_CRC_P3:
		{
			panel_ext = mtk_dsi_get_panel_ext(comp);
			if (!(panel_ext && panel_ext->funcs &&
				  panel_ext->funcs->panel_set_flat_crc_p3))
				break;
			pr_info("Flat CRC p3");
			panel_ext->funcs->panel_set_flat_crc_p3(dsi->panel);
			break;
		}

		case DISPPARAM_BACKLIGHT_SET:
		{
			panel_ext = mtk_dsi_get_panel_ext(comp);
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->setbacklight_control))
				break;

			backlight_by_brightness = *(u32 *)params = (cmd & 0x7ff);
			if (!backlight_by_brightness) {
				dsi->normal_hbm_flag = false;
			}
			pr_info("fod_backlight_flag = %d(%d), backlight = %d \n",
				dsi->fod_backlight_flag, dsi->normal_hbm_flag, *(u32 *)params);
			panel_ext->funcs->setbacklight_control(dsi->panel, *(u32 *)params, false);

			break;
		}

		case DISPPARAM_PANEL_ID_GET:
		{
			panel_ext = mtk_dsi_get_panel_ext(comp);
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->panel_id_get))
				break;

			panel_ext->funcs->panel_id_get(dsi->panel);
			break;
		}
		case DISPPARAM_SET_THERMAL_HBM_DISABLE:
			pr_info("%s, set thermal hbm disable\n", __func__);
			dsi->thermal_hbm_disabled = true;
			break;
		case DISPPARAM_CLEAR_THERMAL_HBM_DISABLE:
			pr_info("%s, clear thermal hbm disable\n", __func__);
			dsi->thermal_hbm_disabled = false;
			break;

		default:
			break;
	}

	cmd_temp = cmd & 0xF0000000;
	switch (cmd_temp) {
		case DISPPARAM_DOZE_STATE:
		{
			doze_state = cmd & 0x0F;
			if ((0 == doze_state || 1 == doze_state)) {
				dsi->doze_flag = false;
			}
			else {
				dsi->doze_flag = true;
			}

			pr_info("doze_state = %d\n", doze_state);
			g_notify_data.data = &doze_state;
			drm_notifier_call_chain(DRM_EVENT_BLANK, &g_notify_data);

			panel_ext = mtk_dsi_get_panel_ext(comp);
			if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->aod_set_state)) {
				pr_info("func aod_set_state not defined\n");
				break;
			} else if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->get_unset_doze_brightness)) {
				pr_info("func get_unset_doze_brightness not defined\n");
				break;
			} else if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->set_doze_brightness)) {
				pr_info("func set_doze_brightness not defined\n");
				break;
			} else if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->hbm_get_state)) {
				pr_info("func hbm_get_state not defined\n");
				break;
			} else if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->set_nolp)) {
				pr_info("func set_nolp not defined\n");
				break;
			} else {
				/* normal/fod -> aod */
				if (doze_state == DRM_BLANK_LP1 || doze_state == DRM_BLANK_LP2) {
					panel_power_mode = doze_state;

					panel_ext->funcs->get_unset_doze_brightness(dsi->panel, &unset_doze_brightness);
					fod_backlight_flag = mtk_fod_backlight_flag(dsi->panel->connector);
					panel_ext->funcs->hbm_get_state(dsi->panel, &hbm_en);

					pr_info("%s hbm_en = %d, fod_backlight_flag = %d, fod_hbm_flag = %d, normal_hbm_flag = %d\n",
												__func__, hbm_en, fod_backlight_flag, dsi->fod_hbm_flag, dsi->normal_hbm_flag);
					if (hbm_en || fod_backlight_flag || dsi->fod_backlight_flag || dsi->fod_hbm_flag || dsi->normal_hbm_flag) {
						in_aod = false;
					} else {
						in_aod = true;
					}
					panel_ext->funcs->aod_set_state(dsi->panel, &in_aod);

					if (unset_doze_brightness == DOZE_BRIGHTNESS_HBM || unset_doze_brightness == DOZE_BRIGHTNESS_LBM) {
						if (!(hbm_en || fod_backlight_flag || dsi->fod_backlight_flag || dsi->fod_hbm_flag || dsi->normal_hbm_flag)) {
							panel_ext->funcs->set_doze_brightness(dsi->panel, unset_doze_brightness);
						}
					}
				/* AOD->normal */
				} else if (doze_state == DRM_BLANK_UNBLANK) {
						in_aod = false;
						panel_power_mode = doze_state;
						panel_ext->funcs->aod_set_state(dsi->panel, &in_aod);
						panel_ext->funcs->set_nolp(dsi->panel);
				}
			}
			break;
		}
		case DISPPARAM_RESTORE_BACKLIGHT:
		{
			panel_ext = mtk_dsi_get_panel_ext(comp);
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->esd_restore_backlight)) {
				pr_info("esd_restore_backlight func not defined!");
				break;
			} else {
				pr_info("esd_restore_backlight");
				panel_ext->funcs->esd_restore_backlight(dsi->panel, dsi,
					mipi_dsi_dcs_write_gce2, NULL);
			}
			break;
		}
#ifdef CONFIG_DRM_DFPS
		case DISPPARAM_DFPS_STATE:
		{
			dfps_en = cmd & 0x0F;
			pr_info("dfps_en = %d\n", dfps_en);
			mtkfb_enable_idlemgr_dfps(dfps_en);

			break;
		}
#endif
		default:
			break;
	}

	if (need_dsi_lock) {
		pr_info("%s mutex unlock\n", __func__);
		mutex_unlock(&dsi->dsi_lock);
	} else {
		pr_info("%s esd recovery not need mutex unlock\n", __func__);
	}
	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_dsi_funcs = {
	.prepare = mtk_dsi_ddp_prepare,
	.unprepare = mtk_dsi_ddp_unprepare,
	.config_trigger = mtk_dsi_config_trigger,
	.io_cmd = mtk_dsi_io_cmd,
	.is_busy = mtk_dsi_is_busy,
	.io_cmd_dispparam = mtk_dsi_io_cmd_dispparam,
};

static int mtk_dsi_bind(struct device *dev, struct device *master, void *data)
{
	int ret;
	struct drm_device *drm = data;
	struct mtk_dsi *dsi = dev_get_drvdata(dev);

	DDPINFO("%s+\n", __func__);
	ret = mtk_ddp_comp_register(drm, &dsi->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	ret = mtk_dsi_create_conn_enc(drm, dsi);
	if (ret) {
		DRM_ERROR("Encoder create failed with %d\n", ret);
		goto err_unregister;
	}

	DDPINFO("%s-\n", __func__);
	return 0;

err_unregister:
	mipi_dsi_host_unregister(&dsi->host);
	mtk_ddp_comp_unregister(drm, &dsi->ddp_comp);
	return ret;
}

static void mtk_dsi_unbind(struct device *dev, struct device *master,
			   void *data)
{
	struct drm_device *drm = data;
	struct mtk_dsi *dsi = dev_get_drvdata(dev);

	mtk_dsi_destroy_conn_enc(dsi);
	mipi_dsi_host_unregister(&dsi->host);
	mtk_ddp_comp_unregister(drm, &dsi->ddp_comp);
}

static const struct component_ops mtk_dsi_component_ops = {
	.bind = mtk_dsi_bind, .unbind = mtk_dsi_unbind,
};

static const struct mtk_dsi_driver_data mt8173_dsi_driver_data = {
	.reg_cmdq_ofs = 0x200, .irq_handler = mtk_dsi_irq,
	.support_shadow = false,
};

static const struct mtk_dsi_driver_data mt6779_dsi_driver_data = {
	.reg_cmdq_ofs = 0x200,
	.poll_for_idle = mtk_dsi_poll_for_idle,
	.irq_handler = mtk_dsi_irq_status,
	.esd_eint_compat = "mediatek, DSI_TE-eint",
	.support_shadow = false,
};

static const struct mtk_dsi_driver_data mt6885_dsi_driver_data = {
	.reg_cmdq_ofs = 0x200,
	.poll_for_idle = mtk_dsi_poll_for_idle,
	.irq_handler = mtk_dsi_irq_status,
	.esd_eint_compat = "mediatek, DSI_TE-eint",
	.support_shadow = false,
};

static const struct mtk_dsi_driver_data mt6873_dsi_driver_data = {
	.reg_cmdq_ofs = 0x200,
	.poll_for_idle = mtk_dsi_poll_for_idle,
	.irq_handler = mtk_dsi_irq_status,
	.esd_eint_compat = "mediatek, DSI_TE-eint",
	.support_shadow = false,
};

static const struct mtk_dsi_driver_data mt6853_dsi_driver_data = {
	.reg_cmdq_ofs = 0x200,
	.poll_for_idle = mtk_dsi_poll_for_idle,
	.irq_handler = mtk_dsi_irq_status,
	.esd_eint_compat = "mediatek, DSI_TE-eint",
	.support_shadow = false,
};

static const struct mtk_dsi_driver_data mt6833_dsi_driver_data = {
	.reg_cmdq_ofs = 0x200,
	.poll_for_idle = mtk_dsi_poll_for_idle,
	.irq_handler = mtk_dsi_irq_status,
	.esd_eint_compat = "mediatek, DSI_TE-eint",
	.support_shadow = false,
};

static const struct mtk_dsi_driver_data mt2701_dsi_driver_data = {
	.reg_cmdq_ofs = 0x180, .irq_handler = mtk_dsi_irq,
	.support_shadow = false,
};

static const struct of_device_id mtk_dsi_of_match[] = {
	{.compatible = "mediatek,mt2701-dsi", .data = &mt2701_dsi_driver_data},
	{.compatible = "mediatek,mt6779-dsi", .data = &mt6779_dsi_driver_data},
	{.compatible = "mediatek,mt8173-dsi", .data = &mt8173_dsi_driver_data},
	{.compatible = "mediatek,mt6885-dsi", .data = &mt6885_dsi_driver_data},
	{.compatible = "mediatek,mt6873-dsi", .data = &mt6873_dsi_driver_data},
	{.compatible = "mediatek,mt6853-dsi", .data = &mt6853_dsi_driver_data},
	{.compatible = "mediatek,mt6833-dsi", .data = &mt6833_dsi_driver_data},
	{},
};

static int mtk_dsi_probe(struct platform_device *pdev)
{
	struct mtk_dsi *dsi;
	struct device *dev = &pdev->dev;
	const struct of_device_id *of_id;
	struct device_node *remote_node, *endpoint;
	struct resource *regs;
	int irq_num;
	int comp_id;
	int ret;

	DDPINFO("%s+\n", __func__);
	dsi = devm_kzalloc(dev, sizeof(*dsi), GFP_KERNEL);
	if (!dsi)
		return -ENOMEM;
	g_mtk_dsi = dsi;
	pr_info("%s-%d:dsi = %p, ddp_comp = %p \n",__func__, __LINE__, dsi, &dsi->ddp_comp);
	dsi->host.ops = &mtk_dsi_ops;
	dsi->host.dev = dev;

	mutex_init(&dsi->dsi_lock);

	ret = mipi_dsi_host_register(&dsi->host);
	if (ret < 0) {
		dev_err(dev, "failed to register DSI host: %d\n", ret);
		return -EPROBE_DEFER;
	}
	of_id = of_match_device(mtk_dsi_of_match, &pdev->dev);
	if (!of_id) {
		dev_err(dev, "DSI device match failed\n");
		return -EPROBE_DEFER;
	}

	dsi->driver_data = (struct mtk_dsi_driver_data *)of_id->data;

	endpoint = of_graph_get_next_endpoint(dev->of_node, NULL);
	if (endpoint) {
		remote_node = of_graph_get_remote_port_parent(endpoint);
		if (!remote_node) {
			dev_err(dev, "No panel connected\n");
			ret = -ENODEV;
			goto error;
		}

		dsi->bridge = of_drm_find_bridge(remote_node);
		dsi->panel = of_drm_find_panel(remote_node);
		of_node_put(remote_node);
		if (!dsi->bridge && !dsi->panel) {
			dev_info(dev, "Waiting for bridge or panel driver\n");
			ret = -EPROBE_DEFER;
			goto error;
		}
	}

	dsi->ext = find_panel_ext(dsi->panel);

	dsi->engine_clk = devm_clk_get(dev, "engine");
	if (IS_ERR(dsi->engine_clk)) {
		ret = PTR_ERR(dsi->engine_clk);
		dev_err(dev, "Failed to get engine clock: %d\n", ret);
#ifndef CONFIG_FPGA_EARLY_PORTING
		goto error;
#endif
	}

	dsi->digital_clk = devm_clk_get(dev, "digital");
	if (IS_ERR(dsi->digital_clk)) {
		ret = PTR_ERR(dsi->digital_clk);
		dev_err(dev, "Failed to get digital clock: %d\n", ret);
#ifndef CONFIG_FPGA_EARLY_PORTING
		goto error;
#endif
	}

	dsi->hs_clk = devm_clk_get(dev, "hs");
	if (IS_ERR(dsi->hs_clk)) {
		ret = PTR_ERR(dsi->hs_clk);
		dev_err(dev, "Failed to get hs clock: %d\n", ret);
#ifndef CONFIG_FPGA_EARLY_PORTING
		goto error;
#endif
	}

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dsi->regs = devm_ioremap_resource(dev, regs);
	if (IS_ERR(dsi->regs)) {
		ret = PTR_ERR(dsi->regs);
		dev_err(dev, "Failed to ioremap memory: %d\n", ret);
#ifndef CONFIG_FPGA_EARLY_PORTING
		goto error;
#endif
	}

	dsi->phy = devm_phy_get(dev, "dphy");
	if (IS_ERR(dsi->phy)) {
		ret = PTR_ERR(dsi->phy);
		dev_err(dev, "Failed to get MIPI-DPHY: %d\n", ret);
		goto error;
	}

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DSI);
	if (comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		ret = comp_id;
		goto error;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &dsi->ddp_comp, comp_id,
				&mtk_dsi_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		goto error;
	}

	/* init wq */
	init_dsi_wq(dsi);

	irq_num = platform_get_irq(pdev, 0);
	if (irq_num < 0) {
		dev_err(&pdev->dev, "failed to request dsi irq resource\n");
		ret = -EPROBE_DEFER;
		goto error;
	}

	irq_set_status_flags(irq_num, IRQ_TYPE_LEVEL_HIGH);
	ret = devm_request_irq(
		&pdev->dev, irq_num, dsi->driver_data->irq_handler,
		IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(&pdev->dev), dsi);
	if (ret) {
		DDPAEE("%s:%d, failed to request irq:%d ret:%d\n",
				__func__, __LINE__,
				irq_num, ret);
		ret = -EPROBE_DEFER;
		goto error;
	}

	init_waitqueue_head(&dsi->irq_wait_queue);
#ifndef CONFIG_FPGA_EARLY_PORTING
	/* set ccf reference cnt = 1 */
	phy_power_on(dsi->phy);
	ret = clk_prepare_enable(dsi->engine_clk);
	if (ret < 0)
		pr_info("%s Failed to enable engine clock: %d\n",
			__func__, ret);

	ret = clk_prepare_enable(dsi->digital_clk);
	if (ret < 0)
		pr_info("%s Failed to enable digital clock: %d\n",
			__func__, ret);
#endif

#ifdef CONFIG_HWCONF_MANAGER
		dsi_panel_count_init(dsi->panel);
#endif

	dsi->output_en = true;
	dsi->clk_refcnt = 1;
	dsi->esd_recovery_flag = false;
	dsi->thermal_hbm_disabled = false;

	platform_set_drvdata(pdev, dsi);
	DDPINFO("%s-\n", __func__);

	return component_add(&pdev->dev, &mtk_dsi_component_ops);

error:
	mipi_dsi_host_unregister(&dsi->host);
	return -EPROBE_DEFER;
}

static int mtk_dsi_remove(struct platform_device *pdev)
{
	struct mtk_dsi *dsi = platform_get_drvdata(pdev);

	mtk_output_dsi_disable(dsi, false);
	component_del(&pdev->dev, &mtk_dsi_component_ops);

	return 0;
}

struct platform_driver mtk_dsi_driver = {
	.probe = mtk_dsi_probe,
	.remove = mtk_dsi_remove,
	.driver = {

			.name = "mtk-dsi", .of_match_table = mtk_dsi_of_match,
		},
};

/* ***************** PanelMaster ******************* */

u32 fbconfig_mtk_dsi_get_lanes_num(struct mtk_ddp_comp *comp)
{

	struct mtk_dsi *dsi = container_of(comp, struct mtk_dsi, ddp_comp);

	return dsi->lanes;

}
int pm_mtk_dsi_get_mode_type(struct mtk_dsi *dsi)
{
	u32 vid_mode = CMD_MODE;

	if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO) {
		if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_BURST)
			vid_mode = BURST_MODE;
		else if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_SYNC_PULSE)
			vid_mode = SYNC_PULSE_MODE;
		else
			vid_mode = SYNC_EVENT_MODE;
	}

	return vid_mode;
}

int fbconfig_mtk_dsi_get_mode_type(struct mtk_ddp_comp *comp)
{
	struct mtk_dsi *dsi = container_of(comp, struct mtk_dsi, ddp_comp);

	u32 vid_mode = pm_mtk_dsi_get_mode_type(dsi);

	return vid_mode;
}


u32 PanelMaster_get_dsi_timing(struct mtk_dsi *dsi, enum MIPI_SETTING_TYPE type)
{
	u32 dsi_val = 0;
	u32 vid_mode;
	u32 t_hsa;
	int fbconfig_dsiTmpBufBpp = 0;
	struct mtk_panel_ext *ext = dsi->ext;
	struct videomode *vm = &dsi->vm;
	struct dynamic_mipi_params *dyn = NULL;

	if (ext && ext->params)
		dyn = &ext->params->dyn;

	if (dsi->format == MIPI_DSI_FMT_RGB565)
		fbconfig_dsiTmpBufBpp = 2;
	else
		fbconfig_dsiTmpBufBpp = 3;

	vid_mode = pm_mtk_dsi_get_mode_type(dsi);


	t_hsa = (dsi->mipi_hopping_sta) ?
			((dyn && !!dyn->hsa) ?
			dyn->hsa : vm->hsync_len) :
			vm->hsync_len;

	switch (type) {
	case MIPI_LPX:
	{
		dsi_val = readl(dsi->regs + DSI_PHY_TIMECON0);
		dsi_val &= LPX;
		return dsi_val >> 0;
	}
	case MIPI_HS_PRPR:
	{
		dsi_val = readl(dsi->regs + DSI_PHY_TIMECON0);
		dsi_val &= HS_PREP;
		return dsi_val >> 8;
	}
	case MIPI_HS_ZERO:
	{
		dsi_val = readl(dsi->regs + DSI_PHY_TIMECON0);
		dsi_val &= HS_ZERO;
		return dsi_val >> 16;
	}
	case MIPI_HS_TRAIL:
	{
		dsi_val = readl(dsi->regs + DSI_PHY_TIMECON0);
		dsi_val &= HS_TRAIL;
		return dsi_val >> 24;
	}
	case MIPI_TA_GO:
	{
		dsi_val = readl(dsi->regs + DSI_PHY_TIMECON1);
		dsi_val &= TA_GO;
		return dsi_val >> 0;
	}
	case MIPI_TA_SURE:
	{
		dsi_val = readl(dsi->regs + DSI_PHY_TIMECON1);
		dsi_val &= TA_SURE;
		return dsi_val >> 8;
	}
	case MIPI_TA_GET:
	{
		dsi_val = readl(dsi->regs + DSI_PHY_TIMECON1);
		dsi_val &= TA_GET;
		return dsi_val >> 16;
	}
	case MIPI_DA_HS_EXIT:
	{
		dsi_val = readl(dsi->regs + DSI_PHY_TIMECON1);
		dsi_val &= DA_HS_EXIT;
		return dsi_val >> 24;
	}
	case MIPI_CONT_DET:
	{
		dsi_val = readl(dsi->regs + DSI_PHY_TIMECON2);
		dsi_val &= CONT_DET;
		return dsi_val >> 0;
	}
	case MIPI_CLK_ZERO:
	{
		dsi_val = readl(dsi->regs + DSI_PHY_TIMECON2);
		dsi_val &= CLK_ZERO;
		return dsi_val >> 16;
	}
	case MIPI_CLK_TRAIL:
	{
		dsi_val = readl(dsi->regs + DSI_PHY_TIMECON2);
		dsi_val &= CLK_TRAIL;
		return dsi_val >> 24;
	}
	case MIPI_CLK_HS_PRPR:
	{
		dsi_val = readl(dsi->regs + DSI_PHY_TIMECON3);
		dsi_val &= CLK_HS_PREP;
		return dsi_val >> 0;
	}
	case MIPI_CLK_HS_POST:
	{
		dsi_val = readl(dsi->regs + DSI_PHY_TIMECON3);
		dsi_val &= CLK_HS_POST;
		return dsi_val >> 8;
	}
	case MIPI_CLK_HS_EXIT:
	{
		dsi_val = readl(dsi->regs + DSI_PHY_TIMECON3);
		dsi_val &= CLK_HS_EXIT;
		return dsi_val >> 16;
	}
	case MIPI_HPW:
	{
		u32 tmp_hpw;

		tmp_hpw = readl(dsi->regs + DSI_HSA_WC);
		dsi_val = (tmp_hpw + 10) / fbconfig_dsiTmpBufBpp;
		return dsi_val;
	}
	case MIPI_HFP:
	{
		u32 tmp_hfp;

		tmp_hfp = readl(dsi->regs + DSI_HFP_WC);
		dsi_val = (tmp_hfp + 12) / fbconfig_dsiTmpBufBpp;
		return dsi_val;
	}
	case MIPI_HBP:
	{
		u32 tmp_hbp;

		tmp_hbp = readl(dsi->regs + DSI_HBP_WC);
		if (vid_mode == SYNC_EVENT_MODE  || vid_mode == BURST_MODE)
			return (tmp_hbp + 10) / fbconfig_dsiTmpBufBpp - t_hsa;
		else
			return (tmp_hbp + 10) / fbconfig_dsiTmpBufBpp;

	}
	case MIPI_VPW:
	{
		u32 tmp_vpw;

		tmp_vpw = readl(dsi->regs + DSI_VACT_NL);

		return tmp_vpw;
	}
	case MIPI_VFP:
	{
		u32 tmp_vfp;

		tmp_vfp = readl(dsi->regs + DSI_VFP_NL);
		return tmp_vfp;
	}
	case MIPI_VBP:
	{
		u32 tmp_vbp;

		tmp_vbp = readl(dsi->regs + DSI_VBP_NL);
		return tmp_vbp;
	}
	case MIPI_SSC_EN:
	{
		if (dsi->ext->params->ssc_disable)
			dsi_val = 0;
		else
			dsi_val = 1;
		return dsi_val;
	}
	default:
		DDPMSG("fbconfig dsi set timing :no such type!!\n");
		break;
	}

	dsi_val = 0;
	return dsi_val;
}


u32 DSI_ssc_enable(struct mtk_dsi *dsi, u32 en)
{
	u32 disable = en ? 0 : 1;

	dsi->ext->params->ssc_disable = disable;

	return 0;
}
int PanelMaster_DSI_set_timing(struct mtk_dsi *dsi, struct MIPI_TIMING timing)
{

	u32 value;
	int ret = 0;
	u32 vid_mode;
	u32 t_hsa;
	int fbconfig_dsiTmpBufBpp = 0;
	struct mtk_panel_ext *ext = dsi->ext;
	struct videomode *vm = &dsi->vm;
	struct dynamic_mipi_params *dyn = NULL;

	if (ext && ext->params)
		dyn = &ext->params->dyn;

	if (dsi->format == MIPI_DSI_FMT_RGB565)
		fbconfig_dsiTmpBufBpp = 2;
	else
		fbconfig_dsiTmpBufBpp = 3;

	vid_mode = pm_mtk_dsi_get_mode_type(dsi);


	t_hsa = (dsi->mipi_hopping_sta) ?
			((dyn && !!dyn->hsa) ?
			dyn->hsa : vm->hsync_len) :
			vm->hsync_len;

	switch (timing.type) {
	case MIPI_LPX:
	{
		value = readl(dsi->regs + DSI_PHY_TIMECON0);
		value &= 0xffffff00;
		value |= (timing.value << 0);
		writel(value, dsi->regs + DSI_PHY_TIMECON0);
		break;
	}
	case MIPI_HS_PRPR:
	{
		value = readl(dsi->regs + DSI_PHY_TIMECON0);
		value &= 0xffff00ff;
		value |= (timing.value << 8);
		writel(value, dsi->regs + DSI_PHY_TIMECON0);
		break;
	}
	case MIPI_HS_ZERO:
	{
		value = readl(dsi->regs + DSI_PHY_TIMECON0);
		value &= 0xff00ffff;
		value |= (timing.value << 16);
		writel(value, dsi->regs + DSI_PHY_TIMECON0);
		break;
	}
	case MIPI_HS_TRAIL:
	{
		value = readl(dsi->regs + DSI_PHY_TIMECON0);
		value &= 0x00ffffff;
		value |= (timing.value << 24);
		writel(value, dsi->regs + DSI_PHY_TIMECON0);
		break;
	}
	case MIPI_TA_GO:
	{
		value = readl(dsi->regs + DSI_PHY_TIMECON1);
		value &= 0xffffff00;
		value |= (timing.value << 0);
		writel(value, dsi->regs + DSI_PHY_TIMECON1);
		break;
	}
	case MIPI_TA_SURE:
	{
		value = readl(dsi->regs + DSI_PHY_TIMECON1);
		value &= 0xffff00ff;
		value |= (timing.value << 8);
		writel(value, dsi->regs + DSI_PHY_TIMECON1);
		break;
	}
	case MIPI_TA_GET:
	{
		value = readl(dsi->regs + DSI_PHY_TIMECON1);
		value &= 0xff00ffff;
		value |= (timing.value << 16);
		writel(value, dsi->regs + DSI_PHY_TIMECON1);
		break;
	}
	case MIPI_DA_HS_EXIT:
	{
		value = readl(dsi->regs + DSI_PHY_TIMECON1);
		value &= 0x00ffffff;
		value |= (timing.value << 24);
		writel(value, dsi->regs + DSI_PHY_TIMECON1);
		break;
	}
	case MIPI_CONT_DET:
	{
		value = readl(dsi->regs + DSI_PHY_TIMECON2);
		value &= 0xffffff00;
		value |= (timing.value << 0);
		writel(value, dsi->regs + DSI_PHY_TIMECON2);
		break;
	}
	case MIPI_CLK_ZERO:
	{
		value = readl(dsi->regs + DSI_PHY_TIMECON2);
		value &= 0xff00ffff;
		value |= (timing.value << 16);
		writel(value, dsi->regs + DSI_PHY_TIMECON2);
		break;
	}
	case MIPI_CLK_TRAIL:
	{
		value = readl(dsi->regs + DSI_PHY_TIMECON2);
		value &= 0x00ffffff;
		value |= (timing.value << 24);
		writel(value, dsi->regs + DSI_PHY_TIMECON2);
		break;
	}
	case MIPI_CLK_HS_PRPR:
	{
		value = readl(dsi->regs + DSI_PHY_TIMECON3);
		value &= 0xffffff00;
		value |= (timing.value << 0);
		writel(value, dsi->regs + DSI_PHY_TIMECON3);
		break;
	}
	case MIPI_CLK_HS_POST:
	{
		value = readl(dsi->regs + DSI_PHY_TIMECON3);
		value &= 0xffff00ff;
		value |= (timing.value << 8);
		writel(value, dsi->regs + DSI_PHY_TIMECON3);
		break;
	}
	case MIPI_CLK_HS_EXIT:
	{
		value = readl(dsi->regs + DSI_PHY_TIMECON3);
		value &= 0xff00ffff;
		value |= (timing.value << 16);
		writel(value, dsi->regs + DSI_PHY_TIMECON3);
		break;
	}
	case MIPI_HPW:
	{
		timing.value = timing.value * fbconfig_dsiTmpBufBpp - 10;
		timing.value = ALIGN_TO((timing.value), 4);
		writel(timing.value, dsi->regs + DSI_HSA_WC);
		break;
	}
	case MIPI_HFP:
	{
		timing.value = timing.value * fbconfig_dsiTmpBufBpp - 12;
		timing.value = ALIGN_TO(timing.value, 4);
		writel(timing.value, dsi->regs + DSI_HFP_WC);
		break;
	}
	case MIPI_HBP:
	{
		u32 hbp_byte;

		if (vid_mode == SYNC_EVENT_MODE ||
			vid_mode == BURST_MODE) {
			hbp_byte = timing.value + t_hsa;
			hbp_byte = hbp_byte * fbconfig_dsiTmpBufBpp - 10;
		} else {
			hbp_byte = timing.value * fbconfig_dsiTmpBufBpp - 10;
		}
		hbp_byte = ALIGN_TO(hbp_byte, 4);
		writel(hbp_byte, dsi->regs + DSI_HBP_WC);
		break;
	}
	case MIPI_VPW:
	{
		writel(timing.value, dsi->regs + DSI_VACT_NL);
		break;
	}
	case MIPI_VFP:
	{
		writel(timing.value, dsi->regs + DSI_VFP_NL);
		break;
	}
	case MIPI_VBP:
	{
		writel(timing.value, dsi->regs + DSI_VBP_NL);
		break;
	}
	case MIPI_SSC_EN:
	{
		DSI_ssc_enable(dsi, timing.value);
		break;
	}
	default:
		DDPMSG("fbconfig dsi set timing :no such type!!\n");
		break;

	}
	return ret;
}


static int dsi_dcs_write(struct mtk_dsi *dsi, void *data, size_t len)
{
	struct mipi_dsi_device *dsi_device = dsi->dev_for_PM;
	ssize_t ret;
	char *addr;

	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi_device, data, len);
	else
		ret = mipi_dsi_generic_write(dsi_device, data, len);

	return ret;
}

static int dsi_dcs_read(struct mtk_dsi *dsi,
	uint8_t cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi_device = dsi->dev_for_PM;
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi_device, cmd, data, len);

	return ret;
}

int fbconfig_get_esd_check(struct mtk_dsi *dsi, uint32_t cmd,
						uint8_t *buffer, uint32_t num)
{
	int array[4];
	int ret = 0;
	/* set max returen packet size */
	/* array[0] = 0x00013700 */
	array[0] = 0x3700 + (num << 16);
	ret = dsi_dcs_write(dsi, array, 1);
	if (ret < 0) {
		DDPPR_ERR("fail to writing seq\n");
		return -1;
	}
	ret = dsi_dcs_read(dsi, cmd, buffer, num);
	if (ret < 0) {
		DDPPR_ERR("fail to read seq\n");
		return -1;
	}

	return 0;
}

int fbconfig_get_esd_check_test(struct drm_crtc *crtc,
	uint32_t cmd, uint8_t *buffer, uint32_t num)
{

	int ret = 0;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;
	struct mtk_dsi *dsi;
	struct mtk_panel_params *dsi_params;
	int cmd_matched = 0;
	uint32_t i = 0;

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
	if (crtc->state && !(crtc->state->active)) {
		DDPMSG("%s:crtc is inactive  -- skip\n", __func__);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		goto done;
	}

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (unlikely(!output_comp)) {

		DDPPR_ERR("%s: invalid output comp\n", __func__);
		ret = -EINVAL;
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		goto done;
	}
	dsi = container_of(output_comp, struct mtk_dsi, ddp_comp);
	if (dsi && dsi->ext && dsi->ext->params)
		dsi_params = dsi->ext->params;//get_dsi_params_handle((uint32_t)(PM_DSI0));
	if (dsi && dsi_params) {
		for (i = 0; i < ESD_CHECK_NUM; i++) {
			if (dsi_params->lcm_esd_check_table[i].cmd == 0)
				break;
			if ((uint32_t)(dsi_params->lcm_esd_check_table[i].cmd) == cmd) {
				cmd_matched = 1;
				break;
			}
		}
	} else {
		DDPPR_ERR("%s: dsi or panel is invalid  -- skip\n", __func__);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		goto done;
	}
	if (!cmd_matched) {
		DDPPR_ERR("%s: cmd not matched support cmd=%d, test cmd =%d -- skip\n", __func__,
				dsi_params->lcm_esd_check_table[0].cmd, cmd);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		goto done;
	}
	mtk_drm_idlemgr_kick(__func__, &mtk_crtc->base, 0);

	/* 0 disable esd check */
	if (mtk_drm_lcm_is_connect())
		mtk_disp_esd_check_switch(crtc, false);

	/* 1 stop crtc */
	mtk_crtc_stop_for_pm(mtk_crtc, true);

	/* 2 stop dsi */
	mtk_dsi_stop(dsi);
	mtk_dsi_clk_hs_mode(dsi, 0);

	mtk_dsi_set_interrupt_enable(dsi);
	/* 3 read lcm esd check */
	ret = fbconfig_get_esd_check(dsi, cmd, buffer, num);

	/* 4 start crtc */
	mtk_crtc_start_for_pm(crtc);
	/* 5 start dsi */
	mtk_dsi_clk_hs_mode(dsi, 1);
	mtk_dsi_start(dsi);

	/* 6 enable esd check */
	if (mtk_drm_lcm_is_connect())
		mtk_disp_esd_check_switch(crtc, true);

	mtk_crtc_hw_block_ready(crtc);
	if (mtk_crtc_is_frame_trigger_mode(crtc)) {

		struct cmdq_pkt *cmdq_handle;

		mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
				mtk_crtc->gce_obj.client[CLIENT_CFG]);

		cmdq_pkt_set_event(cmdq_handle,
				mtk_crtc->gce_obj.event[EVENT_STREAM_DIRTY]);
		cmdq_pkt_set_event(cmdq_handle,
				mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
		cmdq_pkt_set_event(cmdq_handle,
				mtk_crtc->gce_obj.event[EVENT_ESD_EOF]);

		cmdq_pkt_flush(cmdq_handle);
		cmdq_pkt_destroy(cmdq_handle);
	}

	mtk_drm_idlemgr_kick(__func__, &mtk_crtc->base, 0);
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

done:
	return ret;
}


void Panel_Master_primary_display_config_dsi(struct mtk_dsi *dsi,
	const char *name, uint32_t config_value)
{
	unsigned long mipi_tx_rate;

	if (!strcmp(name, "PM_CLK")) {
		pr_debug("Pmaster_config_dsi: PM_CLK:%d\n", config_value);
		dsi->ext->params->pll_clk = config_value;
	} else if (!strcmp(name, "PM_SSC")) {
		pr_debug("Pmaster_config_dsi: PM_SSC:%d\n", config_value);
		dsi->ext->params->ssc_range = config_value;
		return;
	}

	dsi->data_rate = dsi->ext->params->pll_clk * 2;
	mipi_tx_rate = dsi->data_rate * 1000000;

	mtk_dsi_set_interrupt_enable(dsi);
	/* config dsi clk */

	clk_set_rate(dsi->hs_clk, mipi_tx_rate);
	mtk_mipi_tx_pll_rate_set_adpt(dsi->phy, dsi->data_rate);

	mtk_dsi_phy_timconfig(dsi, NULL);

	if (!mtk_dsi_is_cmd_mode(&dsi->ddp_comp)) {
		mtk_dsi_set_vm_cmd(dsi);
		mtk_dsi_calc_vdo_timing(dsi);
		mtk_dsi_config_vdo_timing(dsi);
	}

}

u32 PanelMaster_get_CC(struct mtk_dsi *dsi)
{

	u32 tmp_reg;

	tmp_reg = readl(dsi->regs + DSI_TXRX_CTRL);
	tmp_reg &= HSTX_CKLP_EN;
	return (tmp_reg >> 16);

}


void PanelMaster_set_CC(struct mtk_dsi *dsi, u32 enable)
{
	u32 tmp_reg;

	DDPMSG("set_cc :%d\n", enable);
	tmp_reg = readl(dsi->regs + DSI_TXRX_CTRL);
	tmp_reg &= (~HSTX_CKLP_EN);
	tmp_reg |= (enable << 16);
	writel(tmp_reg, dsi->regs + DSI_TXRX_CTRL);
}

struct mtk_dsi *pm_get_mtk_dsi(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp = NULL;
	struct mtk_dsi *dsi = NULL;

	if (crtc->state && !(crtc->state->active)) {
		DDPMSG("%s: crtc is inactive  -- skip\n", __func__);
		return dsi;
	}

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (unlikely(!output_comp)) {
		DDPPR_ERR("%s: invalid output comp\n", __func__);
		return dsi;
	}
	dsi = container_of(output_comp, struct mtk_dsi, ddp_comp);
	return dsi;
}

int Panel_Master_dsi_config_entry(struct drm_crtc *crtc,
	const char *name, int config_value)
{
	int ret = 0;
	struct mtk_dsi *dsi = NULL;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	dsi = pm_get_mtk_dsi(crtc);
	if (!dsi) {
		ret = -EINVAL;
		goto done;
	}
	mtk_drm_idlemgr_kick(__func__, &mtk_crtc->base, 0);

	/*  disable esd check */
	if (mtk_drm_lcm_is_connect())
		mtk_disp_esd_check_switch(crtc, false);

	if ((!strcmp(name, "PM_CLK")) || (!strcmp(name, "PM_SSC"))) {
		Panel_Master_primary_display_config_dsi(dsi,
			name, config_value);
	} else if (!strcmp(name, "PM_DRIVER_IC_RESET") && (!config_value)) {
		if (dsi->panel) {
			if (drm_panel_prepare(dsi->panel))
				DDPPR_ERR("failed to enable the panel\n");
		}
	}
	/* enable esd check */
	if (mtk_drm_lcm_is_connect())
		mtk_disp_esd_check_switch(crtc, true);


done:

	return ret;
}

int Panel_Master_lcm_get_dsi_timing_entry(struct drm_crtc *crtc,
	int type)
{
	int ret = 0;
	struct mtk_dsi *dsi = NULL;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	dsi = pm_get_mtk_dsi(crtc);
	if (!dsi) {
		ret = -EINVAL;
		goto done;
	}
	mtk_drm_idlemgr_kick(__func__, &mtk_crtc->base, 0);

	ret = PanelMaster_get_dsi_timing(dsi, type);


done:
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
	return ret;
}

int Panel_Master_mipi_set_timing_entry(struct drm_crtc *crtc,
	struct MIPI_TIMING timing)
{
	int ret = 0;
	struct mtk_dsi *dsi = NULL;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	dsi = pm_get_mtk_dsi(crtc);
	if (!dsi) {
		ret = -EINVAL;
		goto done;
	}
	mtk_drm_idlemgr_kick(__func__, &mtk_crtc->base, 0);

	ret = PanelMaster_DSI_set_timing(dsi, timing);

done:
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
	return ret;
}

int Panel_Master_mipi_set_cc_entry(struct drm_crtc *crtc,
	int enable)
{
	int ret = 0;
	struct mtk_dsi *dsi = NULL;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	dsi = pm_get_mtk_dsi(crtc);
	if (!dsi) {
		ret = -EINVAL;
		goto done;
	}
	mtk_drm_idlemgr_kick(__func__, &mtk_crtc->base, 0);

	PanelMaster_set_CC(dsi, enable);

done:
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
	return ret;
}

int Panel_Master_mipi_get_cc_entry(struct drm_crtc *crtc)
{
	int ret = 0;
	struct mtk_dsi *dsi = NULL;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	dsi = pm_get_mtk_dsi(crtc);
	if (!dsi) {
		ret = -EINVAL;
		goto done;
	}
	mtk_drm_idlemgr_kick(__func__, &mtk_crtc->base, 0);

	ret = PanelMaster_get_CC(dsi);

done:
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
	return ret;
}
/* ******************* end PanelMaster ***************** */
