################################################################################
#
# flutter-embedded-linux
#
################################################################################

FLUTTER_EMBEDDED_LINUX_VERSION = cb4b5fff73
# FLUTTER_EMBEDDED_LINUX_SITE = https://github.com/sony/flutter-embedded-linux.git
# FLUTTER_EMBEDDED_LINUX_SITE_METHOD = git
FLUTTER_EMBEDDED_LINUX_SITE = $(call github,sony,flutter-embedded-linux,$(FLUTTER_EMBEDDED_LINUX_VERSION))
FLUTTER_EMBEDDED_LINUX_LICENSE = BSD-3-Clause
FLUTTER_EMBEDDED_LINUX_LICENSE_FILES = LICENSE
FLUTTER_EMBEDDED_LINUX_DEPENDENCIES = \
	flutter-engine \
	libxkbcommon \
	libgles \
	libdrm

define FLUTTER_EMBEDDED_LINUX_LINK_ENGINE
	mkdir -p $(@D)/build/
	$(INSTALL) -D -m 0755 $(STAGING_DIR)/usr/lib/libflutter_engine.so \
		$(@D)/build/
endef
FLUTTER_EMBEDDED_LINUX_PRE_BUILD_HOOKS += FLUTTER_EMBEDDED_LINUX_LINK_ENGINE

FLUTTER_EMBEDDED_LINUX_CONF_OPTS += \
	-DCMAKE_BUILD_TYPE=Release \
	-DENABLE_ELINUX_EMBEDDER_LOG=ON \
	-DFLUTTER_RELEASE=ON

ifeq ($(BR2_PACKAGE_FLUTTER_EMBEDDED_LINUX_WAYLAND),y)
FLUTTER_EMBEDDED_LINUX_DEPENDENCIES += wayland
FLUTTER_EMBEDDED_LINUX_CONF_OPTS += \
	-DBACKEND_TYPE=WAYLAND \
	-DUSER_PROJECT_PATH=examples/flutter-wayland-client
FLUTTER_EMBEDDED_LINUX_EXAMPLE = flutter-client
else ifeq ($(BR2_PACKAGE_FLUTTER_EMBEDDED_LINUX_X11),y)
FLUTTER_EMBEDDED_LINUX_DEPENDENCIES += libxcb xlib_libX11
FLUTTER_EMBEDDED_LINUX_CONF_OPTS += \
	-DBACKEND_TYPE=X11 \
	-DUSER_PROJECT_PATH=examples/flutter-x11-client
FLUTTER_EMBEDDED_LINUX_EXAMPLE = flutter-x11-client
else ifeq ($(BR2_PACKAGE_FLUTTER_EMBEDDED_LINUX_GBM),y)
FLUTTER_EMBEDDED_LINUX_DEPENDENCIES += libgbm systemd
FLUTTER_EMBEDDED_LINUX_CONF_OPTS += \
	-DBACKEND_TYPE=DRM-GBM \
	-DUSER_PROJECT_PATH=examples/flutter-drm-gbm-backend
FLUTTER_EMBEDDED_LINUX_EXAMPLE = flutter-drm-gbm-backend
endif

ifeq ($(BR2_PACKAGE_FLUTTER_EMBEDDED_LINUX_CLIENT),y)
define FLUTTER_EMBEDDED_LINUX_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/$(FLUTTER_EMBEDDED_LINUX_EXAMPLE) \
		$(TARGET_DIR)/usr/bin/flutter-client
endef
else
FLUTTER_EMBEDDED_LINUX_CONF_OPTS += -DBUILD_ELINUX_SO=ON

define FLUTTER_EMBEDDED_LINUX_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/libflutter_elinux_*.so \
		$(TARGET_DIR)/usr/lib/
endef
endif

$(eval $(cmake-package))
