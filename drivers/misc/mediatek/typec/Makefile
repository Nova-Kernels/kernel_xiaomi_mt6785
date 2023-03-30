

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/inc

ifeq ($(CONFIG_MTK_CHARGER_BEGONIA),y)
	obj-$(CONFIG_TCPC_CLASS) += tcpc_begonia/
else
	obj-$(CONFIG_TCPC_CLASS) += tcpc/
endif

obj-$(CONFIG_MTK_USB_TYPEC_U3_MUX) += u3_switch/
obj-$(CONFIG_MTK_USB_TYPEC_DP_MUX) += dp/
