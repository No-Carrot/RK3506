################################################################################
#
# mimalloc
#
################################################################################

MIMALLOC_VERSION = 81d69d525e07a4e0e65860085c4281fe1cfb8950
MIMALLOC_SITE = $(call github,microsoft,mimalloc,$(MIMALLOC_VERSION))
MIMALLOC_LICENSE = MIT
MIMALLOC_LICENSE_FILES = LICENSE
MIMALLOC_INSTALL_STAGING = YES

MIMALLOC_CONF_OPTS += -DMI_BUILD_TESTS=OFF

ifneq ($(BR2_STATIC_LIBS)$(BR2_PACKAGE_MIMALLOC_STATIC_ONLY),)
MIMALLOC_CONF_OPTS += -DMI_BUILD_SHARED=OFF
endif

$(eval $(cmake-package))
