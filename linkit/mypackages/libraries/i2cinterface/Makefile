include $(TOPDIR)/rules.mk

PKG_NAME:=i2cinterface
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_LICENSE:=GPL-2.0-only

include $(INCLUDE_DIR)/package.mk
include $(TOPDIR)/mypackages/gcc14-legacy.mk

define Package/libi2cinterface
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=I2CINTERFACE LIBRARY
  DEPENDS:=+libmraa +libotdrinterface +libocminterface +liblog-maker +libjson-c
endef

define Package/libi2cinterface/description
  I2C interface library.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)/inc/mraa
	$(CP) $(CURDIR)/../log_maker/files/. $(PKG_BUILD_DIR)/
	$(CP) $(CURDIR)/../otdrinterface/files/inc/. $(PKG_BUILD_DIR)/inc/
	$(CP) $(CURDIR)/../ocminterface/files/inc/. $(PKG_BUILD_DIR)/inc/
	$(CP) $(CURDIR)/../ledinterface/files/inc/. $(PKG_BUILD_DIR)/inc/
	$(CP) ./files/. $(PKG_BUILD_DIR)/
	$(CP) $(CURDIR)/../crc/. $(PKG_BUILD_DIR)/
	$(CP) $(CURDIR)/../common/. $(PKG_BUILD_DIR)/
	$(CP) $(CURDIR)/../transport/powermgtsocket/. $(PKG_BUILD_DIR)/
	$(CP) $(CURDIR)/../powermanagement/. $(PKG_BUILD_DIR)/
	$(CP) $(CURDIR)/../freescaleinterface/. $(PKG_BUILD_DIR)/
	$(CP) $(STAGING_DIR)/usr/include/mraa/*.h $(PKG_BUILD_DIR)/inc/mraa/
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
	$(CP) $(PKG_BUILD_DIR)/inc/*.h $(1)/usr/include/
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_BUILD_DIR)/lib/libi2cinterface.so $(1)/usr/lib/
endef

define Package/libi2cinterface/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_BUILD_DIR)/lib/libi2cinterface.so $(1)/usr/lib/
endef

$(eval $(call BuildPackage,libi2cinterface))
