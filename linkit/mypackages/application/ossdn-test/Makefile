include $(TOPDIR)/rules.mk

PKG_NAME:=ossdntest
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_LICENSE:=GPL-2.0-only

include $(INCLUDE_DIR)/package.mk
include $(TOPDIR)/mypackages/gcc14-legacy.mk

define Package/ossdntest
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=ossdntest - Test Application for Interface Libraries
  DEPENDS:=+libmraa +libocminterface +libotdrinterface +liblog-maker +libhweventcallback +libpthread +libi2cinterface +libledinterface +libbatterymanagement
endef

define Package/ossdntest/description
  Test application for XMF-H interface libraries.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./files/. $(PKG_BUILD_DIR)/
endef

define Build/Configure
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS) -I$(STAGING_DIR)/usr/include -I$(PKG_BUILD_DIR)/inc $(GCC14_LEGACY_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS) -L$(STAGING_DIR)/usr/lib"
endef

define Package/ossdntest/install
	$(INSTALL_DIR) $(1)/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/bin/ossdn-test $(1)/bin/
endef

$(eval $(call BuildPackage,ossdntest))
