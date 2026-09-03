################################################################################
#
# LittleVGL
#
################################################################################

LVGL_VERSION = 8.4.0
LVGL_SITE = $(call github,lvgl,lvgl,v$(LVGL_VERSION))
LVGL_INSTALL_STAGING = YES

ifeq ($(BR2_LV_USE_DEMO_WIDGETS), y)
LVGL_CONF_OPTS += -DLV_USE_DEMO_WIDGETS=1
endif

ifeq ($(BR2_LV_USE_DEMO_KEYPAD_AND_ENCODER), y)
LVGL_CONF_OPTS += -DLV_USE_DEMO_KEYPAD_AND_ENCODER=1
endif

ifeq ($(BR2_LV_USE_DEMO_BENCHMARK), y)
LVGL_CONF_OPTS += -DLV_USE_DEMO_BENCHMARK=1
endif

ifeq ($(BR2_LV_USE_DEMO_STRESS), y)
LVGL_CONF_OPTS += -DLV_USE_DEMO_STRESS=1
endif

ifeq ($(BR2_LV_USE_DEMO_MUSIC), y)
LVGL_CONF_OPTS += -DLV_USE_DEMO_MUSIC=1
endif

ifeq ($(BR2_LV_USE_FREETYPE), y)
LVGL_DEPENDENCIES += freetype
endif

ifeq ($(BR2_LV_USE_GPU_SDL), y)
LVGL_CONF_OPTS += -DLV_USE_GPU_SDL=1
LVGL_DEPENDENCIES += sdl2
endif

define LVGL_PRE_HOOK
	cat $(BR2_CONFIG) | grep "LVGL\|LV_" | grep -v "#" > $(LVGL_DIR)/lv_conf.mk
	$(SED) "s/^/set\(/;s/=y/=1/;s/=/ /;s/$$/)/;s/\"/\"\\\\\"/;s/\")/\\\\\"\")/" $(LVGL_DIR)/lv_conf.mk
endef
LVGL_PRE_CONFIGURE_HOOKS += LVGL_PRE_HOOK

$(eval $(cmake-package))
