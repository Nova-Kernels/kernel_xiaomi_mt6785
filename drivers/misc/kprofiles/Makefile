# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2021-2023 Dakkshesh <dakkshesh5@gmail.com>.

obj-$(CONFIG_KPROFILES)	+=	kprofiles.o

kprofiles-objs	:=	main.o
kprofiles-$(CONFIG_KPROFILES_MTK_BOOST) += mtk_boost.o

ccflags-$(CONFIG_KPROFILES_MTK_BOOST) += \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat \
	-I$(srctree)/drivers/misc/mediatek/base/power/include/ppm_v3
