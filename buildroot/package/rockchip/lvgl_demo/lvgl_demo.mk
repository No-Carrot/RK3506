################################################################################
#
# lvgl_demo
#
################################################################################

LVGL_DEMO_SITE = $(TOPDIR)/../app/lvgl_demo
LVGL_DEMO_SITE_METHOD = local
LVGL_DEMO_IGH_DIR = $(abspath $(TOPDIR)/../external/ethercat_igh)
LVGL_DEMO_IGH_OUT = $(LVGL_DEMO_IGH_DIR)/out

# add dependencies
LVGL_DEMO_DEPENDENCIES += lvgl

LVGL_DEMO_INSTALL_STAGING = YES

ifeq ($(BR2_PACKAGE_LVGL_VERSION_9), y)
LVGL_DEMO_CONF_OPTS += -DLVGL_V9=1
endif

ifeq ($(BR2_LV_USE_DRAW_RK_TRANSFORM), y)
LVGL_DEMO_CONF_OPTS += -DLV_USE_DRAW_RK_TRANSFORM=1
endif

ifeq ($(BR2_LV_USE_MIMALLOC_MALLOC), y)
LVGL_DEMO_CONF_OPTS += -DLV_USE_MIMALLOC_MALLOC=1
endif

ifeq ($(BR2_LVGL_DEMO_WIDGETS), y)
LVGL_DEMO_CONF_OPTS += -DLVGL_DEMO_WIDGETS=1
endif

ifeq ($(BR2_LVGL_DEMO_BENCHMARK), y)
LVGL_DEMO_CONF_OPTS += -DLVGL_DEMO_BENCHMARK=1
endif

ifeq ($(BR2_LVGL_DEMO_MUSIC), y)
LVGL_DEMO_CONF_OPTS += -DLVGL_DEMO_MUSIC=1
endif

ifeq ($(BR2_LVGL_DEMO_RK_DEMO), y)
ifeq ($(BR2_RK_DEMO_ENABLE_MULTIMEDIA), y)
LVGL_DEMO_DEPENDENCIES += rockit rkadk
endif
ifeq ($(BR2_RK_DEMO_ENABLE_WIFIBT), y)
LVGL_DEMO_DEPENDENCIES += rkwifibt-app
endif
LVGL_DEMO_CONF_OPTS += -DRK_DEMO_MULTIMEDIA_EN=$(if $(BR2_RK_DEMO_ENABLE_MULTIMEDIA),1,0)
LVGL_DEMO_CONF_OPTS += -DRK_DEMO_SENSOR_EN=$(if $(BR2_RK_DEMO_ENABLE_SENSOR),1,0)
LVGL_DEMO_CONF_OPTS += -DRK_DEMO_WIFIBT_EN=$(if $(BR2_RK_DEMO_ENABLE_WIFIBT),1,0)
LVGL_DEMO_CONF_OPTS += -DRK_DEMO_ASR_EN=$(if $(BR2_RK_DEMO_ENABLE_ASR),1,0)
LVGL_DEMO_CONF_OPTS += -DLV_USE_RK_DEMO=1
endif

ifeq ($(BR2_LVGL_DEMO_MOTOR_DEMO), y)
LVGL_DEMO_CONF_OPTS += -DLV_USE_MOTOR_DEMO=1
LVGL_DEMO_CONF_OPTS += -DIGH_ETHERCAT_PREFIX=$(LVGL_DEMO_IGH_OUT)

define LVGL_DEMO_INSTALL_IGH_ETHERCAT_LIBRARY
	$(INSTALL) -D -m 0755 $(LVGL_DEMO_IGH_OUT)/lib/libethercat.so.1.1.0 $(TARGET_DIR)/usr/lib/libethercat.so.1.1.0
	ln -sf libethercat.so.1.1.0 $(TARGET_DIR)/usr/lib/libethercat.so.1
	ln -sf libethercat.so.1.1.0 $(TARGET_DIR)/usr/lib/libethercat.so
endef

define LVGL_DEMO_INSTALL_IGH_ETHERCAT_RUNTIME
	kernel_release=`$(MAKE) -s -C $(TOPDIR)/../kernel kernelrelease`; \
	module_dir="$(TARGET_DIR)/lib/modules/$$kernel_release/kernel/drivers/ethercat"; \
	mkdir -p "$$module_dir"; \
	rm -f "$(TARGET_DIR)/lib/modules/$$kernel_release/ec_master.ko" \
		"$(TARGET_DIR)/lib/modules/$$kernel_release/ec_generic.ko" \
		"$(TARGET_DIR)/lib/modules/$$kernel_release/ec_mini.ko"; \
	$(INSTALL) -m 0644 $(LVGL_DEMO_IGH_DIR)/master/ec_master.ko "$$module_dir/ec_master.ko"; \
	$(INSTALL) -m 0644 $(LVGL_DEMO_IGH_DIR)/devices/ec_generic.ko "$$module_dir/ec_generic.ko"; \
	$(INSTALL) -D -m 0755 $(LVGL_DEMO_IGH_OUT)/bin/ethercat $(TARGET_DIR)/usr/bin/ethercat; \
	$(INSTALL) -D -m 0755 $(LVGL_DEMO_IGH_OUT)/sbin/ethercatctl $(TARGET_DIR)/usr/sbin/ethercatctl; \
	$(INSTALL) -D -m 0644 $(LVGL_DEMO_IGH_OUT)/etc/ethercat.conf $(TARGET_DIR)/etc/ethercat.conf; \
	$(INSTALL) -D -m 0644 $(LVGL_DEMO_IGH_OUT)/etc/sysconfig/ethercat $(TARGET_DIR)/etc/sysconfig/ethercat; \
	$(INSTALL) -D -m 0755 $(LVGL_DEMO_IGH_OUT)/etc/init.d/ethercat $(TARGET_DIR)/etc/init.d/ethercat; \
	$(SED) 's|$(LVGL_DEMO_IGH_OUT)/bin/|/usr/bin/|g' \
		-e 's|$(LVGL_DEMO_IGH_OUT)/sbin/|/usr/sbin/|g' \
		-e 's|$(LVGL_DEMO_IGH_OUT)/etc/|/etc/|g' \
		$(TARGET_DIR)/usr/sbin/ethercatctl $(TARGET_DIR)/etc/init.d/ethercat; \
	$(SED) 's/^MASTER0_DEVICE=.*/MASTER0_DEVICE="eth0"/' \
		-e 's/^DEVICE_MODULES=.*/DEVICE_MODULES="generic"/' \
		-e 's/^UPDOWN_INTERFACES=.*/UPDOWN_INTERFACES="eth0"/' \
		-e 's/^MASTER_MODULE_OPTIONS=.*/MASTER_MODULE_OPTIONS="thread_cpu=2 idle_thread_rt_prio=15 op_thread_rt_prio=42"/' \
		$(TARGET_DIR)/etc/ethercat.conf $(TARGET_DIR)/etc/sysconfig/ethercat; \
	for config in $(TARGET_DIR)/etc/ethercat.conf $(TARGET_DIR)/etc/sysconfig/ethercat; do \
		grep -q '^MASTER_MODULE_OPTIONS=' "$$config" || \
			printf '\nMASTER_MODULE_OPTIONS="thread_cpu=2 idle_thread_rt_prio=15 op_thread_rt_prio=42"\n' >> "$$config"; \
	done; \
	if [ -x "$(HOST_DIR)/sbin/depmod" ]; then \
		"$(HOST_DIR)/sbin/depmod" -a -b "$(TARGET_DIR)" "$$kernel_release"; \
	else \
		depmod -a -b "$(TARGET_DIR)" "$$kernel_release"; \
	fi
endef

LVGL_DEMO_POST_INSTALL_TARGET_HOOKS += LVGL_DEMO_INSTALL_IGH_ETHERCAT_LIBRARY
LVGL_DEMO_POST_INSTALL_TARGET_HOOKS += LVGL_DEMO_INSTALL_IGH_ETHERCAT_RUNTIME
endif

ifeq ($(BR2_LVGL_DEMO_BACKEND_SDL), y)
LVGL_DEMO_CONF_OPTS += -DLV_DRV_USE_SDL_GPU=1
LVGL_DEMO_DEPENDENCIES += sdl2
endif

ifeq ($(BR2_LVGL_DEMO_BACKEND_DRM), y)
LVGL_DEMO_CONF_OPTS += -DLV_DRV_USE_DRM=1
LVGL_DEMO_DEPENDENCIES += libdrm libevdev
ifeq ($(BR2_LV_DRM_USE_RGA), y)
LVGL_DEMO_CONF_OPTS += -DLV_USE_RGA=1
LVGL_DEMO_DEPENDENCIES += rockchip-rga
endif
endif

ifeq ($(BR2_LVGL_DEMO_BACKEND_RKADK), y)
LVGL_DEMO_CONF_OPTS += -DLV_DRV_USE_RKADK=1
LVGL_DEMO_DEPENDENCIES += rkadk rockchip-rga libevdev
endif

ifeq ($(BR2_PACKAGE_LV_DRIVERS), y)
LVGL_DEMO_DEPENDENCIES += lv_drivers
ifeq ($(BR2_LV_DRIVERS_USE_OPENGL), y)
LVGL_DEMO_CONF_OPTS += -DLV_DRV_USE_OPENGL=1
endif
endif

ifeq ($(BR2_PACKAGE_RK3506), y)
LVGL_DEMO_CONF_OPTS += -DLVGL_DEMO_RK3506=1
endif

$(eval $(cmake-package))
