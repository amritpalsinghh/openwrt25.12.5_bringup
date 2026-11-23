# ossdn-test

[← Back to Main README](../README.md) · [`.c` / `.h` vs 15.05](./ossdn-test-source-changes.md) · [§7 Troubleshooting](#7-troubleshooting)

Build and run the 15.05 XMF-H CLI **`ossdn-test`** on OpenWrt **v25.12.5** (LinkIt Smart 7688). This file is only about that binary compiling and running on the device.

**Tree:** `openwrt25.12.5_bringup`  
**Feed (only copy):** `linkit/mypackages`  
**Package directory:** `linkit/mypackages/application/ossdn-test`  
**OpenWrt makefile name:** `ossdntest`  
**On the device:** `/bin/ossdn-test`

There is **no** `openwrt25.12.5_bringup/mypackages`. The feed is only `linkit/mypackages`. `additional_files/` is in that feed but **not** selected for `ossdn-test`.

---

## 1. What this is

`ossdn-test` is the product CLI from OpenWrt 15.05. It calls the XMF-H interface libraries (OTDR, OCM, I2C, LED, battery, events).

| Item | Value |
| --- | --- |
| Source | `linkit/mypackages/application/ossdn-test/files/` |
| Feed | `src-link mypackages ../mypackages` in `linkit/feeds.conf` (path is relative to `linkit/feeds/`) |
| Image | `CONFIG_PACKAGE_ossdntest=y` → `/bin/ossdn-test` is in the squashfs |
| APK (current) | `CONFIG_PACKAGE_ossdntest=m` → not in the image; install `ossdntest-*.apk` after flash |

Do not mix `=y` and `=m` for the same package. **Current bringup `.config` uses `=m`.** Libraries stay `=y` in the image so `apk add` has its runtime deps.

---

## 2. Why the 15.05 Makefile is not enough

A raw 15.05 package Makefile does not build on 25.12.5 (GCC 14, musl, `Build/InstallDev`, no `PKG_BUILD_DIR` override). The **package wrappers** under `linkit/mypackages` are the 15.05 sources with the minimum 25.12.5 Makefile/source fixes already proven in `openwrt25_12_5`. CLI behaviour is still 15.05.

`linkit/mypackages/gcc14-legacy.mk` is included as `$(TOPDIR)/mypackages/gcc14-legacy.mk` (`TOPDIR` is `linkit/`).

---

## 3. Dependencies (minimum)

`ossdntest` `DEPENDS` (same as 15.05):

```
+libmraa +libocminterface +libotdrinterface +liblog-maker
+libhweventcallback +libpthread +libi2cinterface
+libledinterface +libbatterymanagement
```

| Package | Where |
| --- | --- |
| `ossdntest` | `linkit/mypackages/application/ossdn-test` |
| `liblog-maker`, `libocminterface`, `libotdrinterface`, `libhweventcallback`, `libi2cinterface`, `libledinterface`, `libbatterymanagement` | `linkit/mypackages/libraries/` |
| `crc`, `common`, `transport`, `powermanagement`, `freescaleinterface` | same `libraries/` dir — **source only** (copied in `Build/Prepare`, not separate packages) |
| `libmraa` | packages feed (not mypackages). Runtime: `libstdcpp`, `libjson-c` |
| `libpthread` | toolchain |

**Not required:** `additional_files` (webui, 15.05 libwebsockets, LinkIt overlay snippets). The 15.05 `libwebsockets` Makefile is renamed so it is not built — see [libwebsockets-disabled.md](./libwebsockets-disabled.md). **Not included:** `otdr_emu`, `xmfhwxx` (uClibc, no musl sources).

`.config` (current):

```
CONFIG_PACKAGE_ossdntest=m
CONFIG_PACKAGE_libmraa=y
CONFIG_PACKAGE_liblog-maker=y
CONFIG_PACKAGE_libocminterface=y
CONFIG_PACKAGE_libotdrinterface=y
CONFIG_PACKAGE_libhweventcallback=y
CONFIG_PACKAGE_libi2cinterface=y
CONFIG_PACKAGE_libledinterface=y
CONFIG_PACKAGE_libbatterymanagement=y
```

`ossdntest=m` = **module / APK only** (not baked into the next sysupgrade). Libs stay `=y` so a flashed image already has `libmraa` and the XMF interface `.so` files. `libmraa-python3` / `python3` are **not** selected.

To put the CLI back inside the image, change `ossdntest=m` to `=y`, `make defconfig`, rebuild the sysupgrade.

---

## 4. One-time feed (after clone; `feeds/` is gitignored)

```bash
cd /home/amrit/XMF_H/openwrt/openwrt25.12.5_bringup/linkit
./scripts/feeds update mypackages
./scripts/feeds update packages
./scripts/feeds install -a -p mypackages
./scripts/feeds install libmraa
make defconfig
grep -E '^CONFIG_PACKAGE_(ossdntest|libmraa|liblog-maker)=' .config
```

Expect `ossdntest=m` and the libs `=y`. Check the feed points at **this** tree:

```bash
ls -l feeds/mypackages/application/ossdn-test
# -> .../linkit/mypackages/application/ossdn-test
```

---

## 5. Compile

No toolchain is in git. First full `make` builds it, then packages.

The **make directory** is `ossdn-test` (folder name). The **package/APK name** is `ossdntest`. Use the folder name in `make package/...`.

### 5.1 Module / APK (current: `ossdntest=m`)

Does **not** put `/bin/ossdn-test` into the next squashfs. Builds an APK you install on the board.

```bash
cd /home/amrit/XMF_H/openwrt/openwrt25.12.5_bringup/linkit
make package/ossdn-test/compile V=s
ls -l bin/packages/mipsel_24kc/mypackages/ossdntest-*.apk
file bin/packages/mipsel_24kc/mypackages/ossdntest-*.apk
```

Copy and install (OpenWrt 25.12.5 uses **apk**, not opkg):

```bash
scp bin/packages/mipsel_24kc/mypackages/ossdntest-*.apk root@192.168.100.1:/tmp/
# on the device (libs already in the image because they are =y):
apk add --allow-untrusted /tmp/ossdntest-*.apk
ossdn-test
ldd /bin/ossdn-test
```

If a lib `.so` is missing, also copy the matching APK from `bin/packages/mipsel_24kc/mypackages/` (and `libmraa` from `bin/packages/mipsel_24kc/packages/`). With the current `.config` that should not be needed on a freshly flashed image.

A leftover `/bin/ossdn-test` in `staging_dir/.../root-ramips/` can remain from an earlier `=y` build. Ignore it. The **next** sysupgrade with `=m` will not include the binary; install the APK instead.

### 5.2 Firmware image (`ossdntest=y`)

Only if you want the CLI inside squashfs:

```bash
sed -i 's/^CONFIG_PACKAGE_ossdntest=m/CONFIG_PACKAGE_ossdntest=y/' .config
make defconfig
make -j"$(nproc)"
```

Host check (valid for `=y` image builds):

```bash
BIN=staging_dir/target-mipsel_24kc_musl/root-ramips/bin/ossdn-test
ls -l "$BIN"
file "$BIN"
# expect MIPS, interpreter ld-musl-mipsel.so.1  (not ld-uClibc)
```

Image: `bin/targets/ramips/mt76x8/openwrt-ramips-mt76x8-mediatek_linkit-smart-7688-squashfs-sysupgrade.bin`  
Flash: `sysupgrade -n`. Console: **ttyS2 57600**.

---

## 6. Run on the device

Same commands as 15.05:

```bash
ossdn-test
ossdn-test hwrev
ossdn-test fslslavechk
ossdn-test productinfo
ossdn-test led
ldd /bin/ossdn-test
```

Also used at boot on the product: `sw3_check`, `op_sw_en`, `boottimeproductinfo`, `power_off_otdr`, plus `otdr` / `ocm` / `occ` / `sfp` / `batterymanagement` / `powermgt` / `eventTest` / `IO_EXP2`.

The binary must **start** (musl). Many subcommands talk to XMF-H I2C/UART; on a board without that hardware they return errors. That is not a failed compile.

Do **not** run `ossdn-test otdr_fw`, `ocm_fw`, `sfp_fw`, or `fslrst` unless you intend to write instrument firmware or reset the Freescale MCU.

---

## 7. Troubleshooting

These are **runtime** CLI errors (same as 15.05). They are not compile/GCC14 issues. Source diffs are in [ossdn-test-source-changes.md](./ossdn-test-source-changes.md).

### 7.1 `ossdn-test productinfo` → `productinfo error: 1201` and empty `msg:`

**Seen on the board:**

```
productinfo error: 1201
and msg:
```

**Meaning**

| Item | Value |
| --- | --- |
| Code | **1201** = `PRODUCT_INFO_FAIL` in `libraries/i2cinterface/files/inc/product_info.h` |
| Command | `ossdn-test productinfo` → `_productInfo()` → `ProductInfo()` in `common_i2c_operation.c` |
| What it does | **Only** `fopen("/tmp/product_info.json", "r")` and copy the file into a buffer. It does **not** talk to I2C, EEPROM, OTDR, or OCM. |
| Empty `msg:` | On `fopen` failure the function sets 1201 and returns. The buffer stays zeros, so `%s` prints nothing. That is 15.05 behaviour, not a musl/GCC bug. |

**Why the file is missing on this image**

`/tmp/product_info.json` is **written** by `ossdn-test boottimeproductinfo` (`initialProductInfo()`). On the 15.05 product image, `xmfh-init-script` (START=99) runs that at boot, so `productinfo` works afterwards.

This bringup image does **not** include `xmfh-init-script`, so nothing creates the JSON until you run `boottimeproductinfo` yourself. `/tmp` is also tmpfs: the file is gone after reboot.

**Check and recover**

```sh
ls -l /tmp/product_info.json
# expect: No such file or directory  →  that is 1201

ossdn-test boottimeproductinfo
cat /tmp/product_info.json
ossdn-test productinfo
```

After a successful populate, `productinfo` should print the JSON (firmware / hardware rev / serial / OTDR / OCM fields). Fields can still be empty if a slave NACKed during `boottimeproductinfo`; that is not 1201.

**If `boottimeproductinfo` itself errors**

That path **does** use I2C (EEPROM `0x56` on bus 0), Freescale UART, OTDR, OCM, IO expander. On a packed XMF-H, expect a JSON file (some fields may be blank). On a **bare LinkIt** (no XMF-H board), I2C/mraa errors are expected — not a failed `ossdn-test` compile.

Do not confuse that with 1201: 1201 only means “cache file missing”.

### 7.2 `make package/ossdntest/compile` → No rule to make target

Use the **directory** name:

```bash
make package/ossdn-test/compile V=s
```

`ossdntest` is the APK/`PKG_NAME`. `ossdn-test` is the folder under `mypackages/application/`.

---

**OpenWrt:** v25.12.5  
**Target:** MediaTek LinkIt Smart 7688  
**Package path:** `linkit/mypackages/application/ossdn-test`
