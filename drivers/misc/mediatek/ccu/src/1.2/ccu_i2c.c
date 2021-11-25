/*
 * Copyright (C) 2016 MediaTek Inc.
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

#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>

#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/sched.h>

#include <linux/printk.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/i2c.h>

#include "i2c-mtk.h"
#include <m4u.h>

#include "ccu_cmn.h"
#include "ccu_i2c.h"
#include "ccu_i2c_hw.h"
#include "kd_camera_feature.h"/*for IMGSENSOR_SENSOR_IDX*/
/**************************************************************************
 *
 **************************************************************************/
 /*I2C Channel offset*/
#define I2C_BASE_OFS_CH1 (0x200)
#define MAX_I2C_CMD_LEN 255
#define CCU_I2C_APDMA_TXLEN 128
#define CCU_I2C_BUS2_HW_DRVNAME  "ccu_i2c_2_hwtrg"
#define CCU_I2C_BUS4_HW_DRVNAME  "ccu_i2c_4_hwtrg"

/*--todo: check if need id-table & name, id of_match_table is given*/
static int ccu_i2c_probe_2(struct i2c_client *client,
	const struct i2c_device_id *id);
static int ccu_i2c_probe_4(struct i2c_client *client,
	const struct i2c_device_id *id);

static int ccu_i2c_remove(struct i2c_client *client);
static uint32_t g_ccu_i2c_id = -1;
static MBOOL ccu_i2c_enabled = MFALSE;
static struct i2c_client *getCcuI2cClient(void);
static struct i2c_client *g_ccuI2cClient2;
static struct i2c_client *g_ccuI2cClient4;
static inline u32 i2c_readl_dma(struct mt_i2c *i2c, u16 offset);
static inline void i2c_writel_dma(u32 value, struct mt_i2c *i2c, u16 offset);
static inline u16 i2c_readw(struct mt_i2c *i2c, u16 offset);
static inline void i2c_writew(u16 value, struct mt_i2c *i2c, u16 offset);

static const struct i2c_device_id ccu_i2c_2_ids[] = {
	{CCU_I2C_BUS2_HW_DRVNAME, 0},
	{}
};
static const struct i2c_device_id ccu_i2c_4_ids[] = {
	{CCU_I2C_BUS4_HW_DRVNAME, 0},
	{}
};


#ifdef CONFIG_OF
static const struct of_device_id ccu_i2c_2_driver_of_ids[] = {
	{.compatible = "mediatek,ccu_sensor_i2c_2_hw",},
	{}
};

static const struct of_device_id ccu_i2c_4_driver_of_ids[] = {
	{.compatible = "mediatek,ccu_sensor_i2c_4_hw",},
	{}
};
#endif

struct i2c_driver ccu_i2c_2_driver = {
	.probe = ccu_i2c_probe_2,
	.remove = ccu_i2c_remove,
	.driver = {
		   .name = CCU_I2C_BUS2_HW_DRVNAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = ccu_i2c_2_driver_of_ids,
#endif
		   },
	.id_table = ccu_i2c_2_ids,
};

struct i2c_driver ccu_i2c_4_driver = {
	.probe = ccu_i2c_probe_4,
	.remove = ccu_i2c_remove,
	.driver = {
		   .name = CCU_I2C_BUS4_HW_DRVNAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = ccu_i2c_4_driver_of_ids,
#endif
		   },
	.id_table = ccu_i2c_4_ids,
};

/*----------------------------------------------------------------------*/
/* CCU Driver: i2c driver funcs                                         */
/*----------------------------------------------------------------------*/
static int ccu_i2c_probe_2(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	/*int i4RetValue = 0;*/
	LOG_DBG("[%s] Attach I2C for HW trriger g_ccuI2cClient2 %p\n",
		"ccu_i2c_probe", client);

	/* get sensor i2c client */
	/*--todo: add subcam implementation*/
	g_ccuI2cClient2 = client;

	/* set I2C clock rate */
	/*#ifdef CONFIG_MTK_I2C_EXTENSION*/
	/*g_pstI2Cclient3->timing = 100;*/ /* 100k */
	/* No I2C polling busy waiting */
	/*g_pstI2Cclient3->ext_flag &= ~I2C_POLLING_FLAG;*/
	/*#endif*/

	LOG_DBG("[ccu_i2c_probe] Attached!!\n");
	return 0;
}

static int ccu_i2c_probe_4(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	/*int i4RetValue = 0;*/
	LOG_DBG("[%s] Attach I2C for HW trriger g_ccuI2cClient4 %p\n",
		"ccu_i2c_probe", client);

	/* get sensor i2c client */
	/*--todo: add subcam implementation*/
	g_ccuI2cClient4 = client;

	/* set I2C clock rate */
	/*#ifdef CONFIG_MTK_I2C_EXTENSION*/
	/*g_pstI2Cclient3->timing = 100;*/ /* 100k */
	/* No I2C polling busy waiting */
	/*g_pstI2Cclient3->ext_flag &= ~I2C_POLLING_FLAG;*/
	/*#endif*/

	LOG_DBG("[ccu_i2c_probe] Attached!!\n");
	return 0;
}

static int ccu_i2c_remove(struct i2c_client *client)
{
	return 0;
}

/*---------------------------------------------------------------------------*/
/* CCU i2c public funcs                                                      */
/*---------------------------------------------------------------------------*/
int ccu_i2c_register_driver(void)
{
	int i2c_ret = 0;

	LOG_DBG("i2c_add_driver(&ccu_i2c_2_driver)++\n");
	i2c_ret = i2c_add_driver(&ccu_i2c_2_driver);
	LOG_DBG("i2c_add_driver(&ccu_i2c_2_driver), ret: %d--\n", i2c_ret);
	LOG_DBG("i2c_add_driver(&ccu_i2c_4_driver)++\n");
	i2c_ret = i2c_add_driver(&ccu_i2c_4_driver);
	LOG_DBG("i2c_add_driver(&ccu_i2c_4_driver), ret: %d--\n", i2c_ret);
	return 0;
}

int ccu_i2c_delete_driver(void)
{
	i2c_del_driver(&ccu_i2c_2_driver);
	i2c_del_driver(&ccu_i2c_4_driver);

	return 0;
}

int ccu_i2c_set_channel(uint32_t i2c_id)
{
	if (i2c_id == 2 || i2c_id == 4) {
		g_ccu_i2c_id = i2c_id;
		return 0;
	} else
		return -EFAULT;
}

int ccu_i2c_buf_mode_init(unsigned char i2c_write_id, int transfer_len)
{
	if (ccu_i2c_buf_mode_en(1) == -1) {
		LOG_DBG("i2c_buf_mode_en fail\n");
		return -1;
	}

	LOG_DBG_MUST("%s done.\n", __func__);

	return 0;
}

int ccu_i2c_buf_mode_en(int enable)
{
	int ret = 0;
	struct i2c_client *pClient = NULL;
	struct mt_i2c *i2c;

	LOG_DBG_MUST("i2c_buf_mode_en %d\n", enable);

	pClient = getCcuI2cClient();

	LOG_DBG("i2c_buf_mode_en, pClient: %p\n", pClient);

	if (pClient == NULL) {
		LOG_ERR("i2c_client is null\n");
		return -1;
	}

	LOG_DBG_MUST("ccu_i2c_enabled %d\n", ccu_i2c_enabled);

	if (enable) {
		if (ccu_i2c_enabled == MFALSE) {
			ret = i2c_ccu_enable(
				pClient->adapter, I2C_BASE_OFS_CH1);
			ccu_i2c_enabled = MTRUE;

			LOG_DBG_MUST("i2c_ccu_enable done(%d).\n", ret);
			i2c = i2c_get_adapdata(pClient->adapter);
			i2c_writew(2, i2c, 0x240);
		}
	} else {
		if (ccu_i2c_enabled == MTRUE) {
			ret = i2c_ccu_disable(pClient->adapter);
			ccu_i2c_enabled = MFALSE;

			LOG_DBG_MUST("i2c_ccu_disable done(%d).\n", ret);
		}
	}
	return ret;
}

int i2c_get_dma_buffer_addr(void **va,
	uint32_t *pa_h, uint32_t *pa_l, uint32_t *i2c_id)
{
	struct i2c_client *pClient = NULL;
	struct mt_i2c *i2c;

	pClient = getCcuI2cClient();

	if (pClient == MNULL) {
		LOG_ERR("ccu client is NULL");
		return -EFAULT;
	}

	i2c = i2c_get_adapdata(pClient->adapter);

	/*i2c_get_dma_buffer_addr_imp(pClient->adapter ,va);*/
	*va = i2c->dma_buf.vaddr + PAGE_SIZE;
	*pa_l = i2c->dma_buf.paddr + PAGE_SIZE;
	*pa_h = 0;
#ifdef CONFIG_COMPAT
	*pa_h = ((i2c->dma_buf.paddr  + PAGE_SIZE) >> 32);
#endif
	*i2c_id = i2c->id;
	LOG_DBG_MUST("va(%p), pal(%d), pah(%d), id(%d)\n",
		*va, *pa_l, *pa_h, *i2c_id);

	return 0;
}

/*-------------------------------------------------------------------*/
/* CCU i2c static funcs                                              */
/*-------------------------------------------------------------------*/
static struct i2c_client *getCcuI2cClient(void)
{
	switch (g_ccu_i2c_id) {
	case 2:
	{
		return g_ccuI2cClient2;
	}
	case 4:
	{
		return g_ccuI2cClient4;
	}
	default:
	{
		return MNULL;
	}
	}
}

static inline u32 i2c_readl_dma(struct mt_i2c *i2c, u16 offset)
{
	return readl(i2c->pdmabase + offset);
}

static inline void i2c_writel_dma(u32 value, struct mt_i2c *i2c, u16 offset)
{
	writel(value, i2c->pdmabase + offset);
}

static inline u16 i2c_readw(struct mt_i2c *i2c, u16 offset)
{
	return readw(i2c->base + offset);
}

static inline void i2c_writew(u16 value, struct mt_i2c *i2c, u16 offset)
{
	writew(value, i2c->base + offset);
}

void ccu_i2c_dump_errr(void)
{
	struct i2c_client *pClient = NULL;
	struct mt_i2c *i2c;

	LOG_DBG_MUST("CCU Dump I2C reg\n");

	pClient = getCcuI2cClient();
	if (pClient == NULL) {
		LOG_ERR("i2c_client is null\n");
		return;
	}
	i2c = i2c_get_adapdata(pClient->adapter);
}