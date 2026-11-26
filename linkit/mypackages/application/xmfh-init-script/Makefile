include $(TOPDIR)/rules.mk

PKG_NAME:=xmfh-init-script
PKG_VERSION:=1.0.0
PKG_RELEASE:=2

PKG_LICENSE:=GPL-2.0-only

include $(INCLUDE_DIR)/package.mk

define Package/xmfh-init-script
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=xmfh init script
endef

define Package/xmfh-init-script/description
  Boot init (rc.common START=99). Seeds /tmp/product_info.json and runs
  ossdn-test boottimeproductinfo so productinfo works after reboot.
  Runtime: /bin/ossdn-test (package ossdntest). Missing 15.05 companions
  (otdr_emu, gaugeSet, wifiwait) are skipped so this still completes.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./files/. $(PKG_BUILD_DIR)/
endef

define Build/Configure
endef

define Build/Compile
	true
endef

define Package/xmfh-init-script/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/xmfh-init-script $(1)/etc/init.d/
	$(INSTALL_DIR) $(1)/usr/lib/xmfh
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wifiwait.sh $(1)/usr/lib/xmfh/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/imageflash.sh $(1)/usr/lib/xmfh/
endef

define Package/xmfh-init-script/postinst
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] && exit 0
/etc/init.d/xmfh-init-script enable
exit 0
endef

$(eval $(call BuildPackage,xmfh-init-script))
