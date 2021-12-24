ifdef MTK_PLATFORM
ifeq ($(strip $(CONFIG_ARM64)), y)
PROJ_DT_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES))
else
PROJ_DT_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM_DTB_OVERLAY_IMAGE_NAMES))
endif

PROJ_DTB_NAMES := $(addsuffix .dtbo,$(PROJ_DT_NAMES))
ABS_DTB_FILES := $(abspath $(addsuffix .dtbo,$(addprefix $(objtree)/arch/$(SRCARCH)/boot/dts/,$(PROJ_DT_NAMES))))

my_dtbo_id := 0
define mk_dtboimg_cfg
echo $(1) >>$(2);\
echo " id=$(my_dtbo_id)" >>$(2);\
$(eval my_dtbo_id:=$(shell echo $$(($(my_dtbo_id)+1))))
endef

dtbs: $(objtree)/dtboimg.cfg

$(objtree)/dtboimg.cfg: FORCE
	rm -f $@.tmp
	$(foreach f,$(ABS_DTB_FILES),$(call mk_dtboimg_cfg,$(f),$@.tmp))
	if ! cmp -s $@.tmp $@; then \
		mv $@.tmp $@; \
	else \
		rm $@.tmp; \
	fi
else
dtbo:

endif#MTK_PLATFORM
