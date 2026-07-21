# SPDX-License-Identifier: GPL-2.0

dtb-$(CONFIG_ARCH_MEDIATEK) += mt2712-evb.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += mt6755-evb.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += mt6795-evb.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += mt6797-evb.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += mt7622-rfb1.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += mt8173-evb.dtb

# begonia/begoniain's shared base SoC tree. Built standalone (not via
# CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE) since that option concatenates
# it onto the kernel image (Image.gz-dtb), which BoardConfig.mk doesn't
# consume -- BOARD_KERNEL_IMAGE_NAME is plain Image.gz. The AOSP build
# picks mt6785.dtb up on its own for boot.img's dedicated v2 dtb section.
dtb-y += mt6785.dtb

ifeq ($(strip $(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE)), y)
dtb-y += $(addsuffix .dtbo, $(notdir $(subst $\",,$(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES))))
endif

always		:= $(dtb-y) $(dtbo-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
