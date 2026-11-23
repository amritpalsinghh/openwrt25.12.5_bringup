include $(TOPDIR)/rules.mk

PKG_NAME:=ledinterface
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_LICENSE:=GPL-2.0-only

include $(INCLUDE_DIR)/package.mk
include $(TOPDIR)/mypackages/gcc14-legacy.mk

define Package/libledinterface
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=LEDINTERFACE LIBRARY
  DEPENDS:=+libmraa +liblog-maker
endef

define Package/libledinterface/description
  LED interface library.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)/inc/mraa
	$(CP) $(CURDIR)/../log_maker/files/. $(PKG_BUILD_DIR)/
	$(CP) ./files/. $(PKG_BUILD_DIR)/
	$(CP) $(CURDIR)/../crc/. $(PKG_BUILD_DIR)/
	$(CP) $(CURDIR)/../common/. $(PKG_BUILD_DIR)/
	$(CP) $(CURDIR)/../freescaleinterface/. $(PKG_BUILD_DIR)/
	$(CP) $(STAGING_DIR)/usr/include/mraa/*.h $(PKG_BUILD_DIR)/inc/mraa/
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
	$(CP) $(PKG_BUILD_DIR)/inc/*.h $(1)/usr/include/
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_BUILD_DIR)/lib/libledinterface.so $(1)/usr/lib/
endef

define Package/libledinterface/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_BUILD_DIR)/lib/libledinterface.so $(1)/usr/lib/
endef

$(eval $(call BuildPackage,libledinterface))
