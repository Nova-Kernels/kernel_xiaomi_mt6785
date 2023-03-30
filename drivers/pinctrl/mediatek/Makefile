# SPDX-License-Identifier: GPL-2.0
# Core
obj-$(CONFIG_PINCTRL_MTK)	+= pinctrl-mtk-common.o
ifneq ($(CONFIG_ARCH_MEDIATEK), n)
ccflags-y += -DCONFIG_PINCTRL_MTK_ALTERNATIVE
endif
obj-$(CONFIG_EINT_MTK)		+= mtk-eint.o


obj-$(CONFIG_PINCTRL_MTK_MOORE) += pinctrl-moore.o pinctrl-mtk-common-v2.o
obj-$(CONFIG_PINCTRL_MTK_PARIS) += pinctrl-paris.o pinctrl-mtk-common-v2.o pinctrl-mtk-common-v2_debug.o

# SoC Drivers
obj-$(CONFIG_PINCTRL_MT2701)	+= pinctrl-mt2701.o
obj-$(CONFIG_PINCTRL_MT8135)	+= pinctrl-mt8135.o
obj-$(CONFIG_PINCTRL_MT8127)	+= pinctrl-mt8127.o
obj-$(CONFIG_PINCTRL_MT8173)	+= pinctrl-mt8173.o
obj-$(CONFIG_PINCTRL_MT6397)	+= pinctrl-mt6397.o
obj-$(CONFIG_PINCTRL_MT6785)	+= pinctrl-mt6785.o
