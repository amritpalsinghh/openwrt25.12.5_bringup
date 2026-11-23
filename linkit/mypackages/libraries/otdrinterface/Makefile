include $(TOPDIR)/rules.mk

PKG_NAME:=otdrinterface
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_LICENSE:=GPL-2.0-only

include $(INCLUDE_DIR)/package.mk
include $(TOPDIR)/mypackages/gcc14-legacy.mk

define Package/libotdrinterface
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=OTDRINTERFACE LIBRARY
  DEPENDS:=+liblog-maker +libpthread +libjson-c
endef

define Package/libotdrinterface/description
  OTDR interface library.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) $(CURDIR)/../log_maker/files/. $(PKG_BUILD_DIR)/
	$(CP) ./files/. $(PKG_BUILD_DIR)/
	$(CP) $(CURDIR)/../transport/serial/. $(PKG_BUILD_DIR)/
	$(CP) $(CURDIR)/../transport/socket/. $(PKG_BUILD_DIR)/
	$(CP) $(CURDIR)/../transport/powermgtsocket/. $(PKG_BUILD_DIR)/
	$(CP) $(CURDIR)/../powermanagement/. $(PKG_BUILD_DIR)/
	$(CP) $(CURDIR)/../HwEventCallback/files/inc/*.h $(PKG_BUILD_DIR)/inc/
	$(CP) $(STAGING_DIR)/usr/include/json-c/*.h $(PKG_BUILD_DIR)/inc/
endef

define Build/Configure
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" \
		AR="$(TARGET_AR)" \
		CFLAGS="$(TARGET_CFLAGS) -fPIC -I$(STAGING_DIR)/usr/include $(GCC14_LEGACY_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS) -L$(STAGING_DIR)/usr/lib"
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include
	$(CP) $(PKG_BUILD_DIR)/inc/otdr_interface.h $(1)/usr/include/
	$(CP) $(PKG_BUILD_DIR)/inc/power_management.h $(1)/usr/include/
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_BUILD_DIR)/lib/libotdrinterface.so $(1)/usr/lib/
endef

define Package/libotdrinterface/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_BUILD_DIR)/lib/libotdrinterface.so $(1)/usr/lib/
endef

$(eval $(call BuildPackage,libotdrinterface))
