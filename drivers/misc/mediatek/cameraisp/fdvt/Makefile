obj-y := fdvt_dummy.o

#$(info FDVT: makefile start)
MTK_CUSTOM_PATH  := $(srctree)/drivers/misc/mediatek

FDVT_50_PLATFORM = mt6785
FDVT_CURRENT_PLATFORM = $(subst ",,$(CONFIG_MTK_PLATFORM))

#$(info FDVT_CURRENT_PLATFORM= $(FDVT_CURRENT_PLATFORM))

ifneq (, $(findstring $(FDVT_CURRENT_PLATFORM), $(FDVT_50_PLATFORM)))
#$(info FDVT: Drv use 5.0 folder)
MTKCAM_FDVT_VERSION := 5.0
MTKCAM_FDVT_PATH := $(MTK_CUSTOM_PATH)/cameraisp/fdvt/5.0
subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/cameraisp/fdvt/5.0/inc
obj-y += 5.0/
endif

ifeq ($(strip $(CONFIG_MTK_ENABLE_GENIEZONE)),y)
ifeq ($(strip $(CONFIG_MTK_CAM_GENIEZONE_SUPPORT)),y)
  MTK_SECURE_FD_SUPPORT := 1
else
  MTK_SECURE_FD_SUPPORT := 0
endif
else
  MTK_SECURE_FD_SUPPORT := 0
endif

subdir-ccflags-y += -DMTK_FD_LARB=$(MTK_FD_LARB)
subdir-ccflags-y += -DMTK_SECURE_FD_SUPPORT=$(MTK_SECURE_FD_SUPPORT)
#$(info FDVT: makefile end)

