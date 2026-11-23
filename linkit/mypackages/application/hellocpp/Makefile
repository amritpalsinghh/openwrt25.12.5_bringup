include $(TOPDIR)/rules.mk

PKG_NAME:=hellocpp
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_LICENSE:=GPL-2.0-only

include $(INCLUDE_DIR)/package.mk

define Package/hellocpp
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Hellocpp -- prints a snarky message
  DEPENDS:=+libstdcpp
endef

define Package/hellocpp/description
  A simple Hello World C++ userspace program for OpenWrt.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/. $(PKG_BUILD_DIR)/
endef

define Build/Configure
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		CXX="$(TARGET_CXX)" \
		CXXFLAGS="$(TARGET_CXXFLAGS) -Wall" \
		LDFLAGS="$(TARGET_LDFLAGS)"
endef

define Package/hellocpp/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/hellocpp $(1)/usr/bin/
endef

$(eval $(call BuildPackage,hellocpp))
