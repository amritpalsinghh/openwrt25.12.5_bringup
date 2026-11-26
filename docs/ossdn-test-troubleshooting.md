# ossdn-test troubleshooting (one issue at a time)

[← Back to Main README](../README.md) · [Build / run](./ossdn-test.md) · [Test plan](./ossdn-test-testplan.md) · [Source changes](./ossdn-test-source-changes.md)

Living log of **runtime** `ossdn-test` problems on OpenWrt v25.12.5 bringup. Each issue is a numbered section: symptom → cause → steps to resolve → how to verify → leftover limits.

This is **not** the GCC 14 / musl compile list. Those edits are in [ossdn-test-source-changes.md](./ossdn-test-source-changes.md). `ossdn_test.c` itself was **not** changed for issue 1 or 2.

Add the next CLI failure as **issue 3**, then 4, … Do not mix issues in one section.

**Tree:** `openwrt25.12.5_bringup`  
**Device binary:** `/bin/ossdn-test` (APK `ossdntest`, `=m`)

---

## Issue 1 — `productinfo` error 1201 (empty `msg:`) and no auto file after reboot

**Status:** resolved on bringup by installing **`xmfh-init-script`** (does not require changing `ossdn_test.c`).  
**Date:** 24 November 2026

### 1.1 Symptom

```
ossdn-test productinfo
productinfo error: 1201
and msg:
```

After a reboot the same command failed again even if `boottimeproductinfo` had been run by hand earlier.

### 1.2 What 1201 means (do not change C for this)

| Item | Value |
| --- | --- |
| Code | **1201** = `PRODUCT_INFO_FAIL` in `libraries/i2cinterface/files/inc/product_info.h` |
| Call path | `ossdn-test productinfo` → `_productInfo()` → `ProductInfo()` in `common_i2c_operation.c` |
| What it does | **Only** `fopen("/tmp/product_info.json", "r")` and copy into a buffer |
| Empty `msg:` | On `fopen` failure the function sets 1201 and returns. The buffer stays zeros, so `%s` prints nothing |

Same as OpenWrt **15.05**. Not a musl/GCC bug. Do **not** edit `ossdn_test.c` to invent a JSON file.

`/tmp` is **tmpfs**. The cache is gone after every reboot unless something recreates it at boot.

### 1.3 Why it failed on bringup

On 15.05, `xmfh-init-script` (`START=99`) does two things that make `productinfo` work after reboot:

1. Seed a blank `/tmp/product_info.json`.
2. Run `ossdn-test boottimeproductinfo` (fills fields from I2C / instruments).

Bringup phase 3 had the CLI APK but **no init script**, so nothing ran those steps at boot.

A second trap: the **unmodified** 15.05 script has:

```sh
test -x /etc/init.d/otdr_emu.sh || exit 0
```

**before** `boottimeproductinfo`. `otdr_emu` is **blocked** on 25.12.5 (uClibc binary, no musl sources). Copying the 15.05 script unchanged would **exit** and still leave `productinfo` at 1201.

### 1.4 Steps to resolve

Do these in order. Do **not** rebuild or flash firmware unless you choose `=y` later.

#### Step A — Confirm it is 1201 (missing file), not a compile fail

On the **device** (UART **ttyS2 57600** if SSH is down):

```sh
which ossdn-test
ldd /bin/ossdn-test
ls -l /tmp/product_info.json
ossdn-test productinfo
```

| Result | Meaning |
| --- | --- |
| `ossdn-test: not found` | Install the CLI APK first ([ossdn-test-install.md](./ossdn-test-install.md)) |
| No `/tmp/product_info.json` and 1201 | This issue — continue |
| File exists but fields empty | Not 1201. `boottimeproductinfo` ran; a slave NACKed. Hardware / later issues |
| Segfault / musl missing `.so` | Compile/install, not this issue |

#### Step B — Prove the writer by hand (no reboot)

```sh
ossdn-test boottimeproductinfo
ls -l /tmp/product_info.json
cat /tmp/product_info.json
ossdn-test productinfo
```

**Expect:** file exists; `productinfo` prints JSON (not 1201). On a **bare LinkIt**, the file may still be the seed/sparse JSON and I2C errors are **N/A**. 1201 must be gone if the file is readable.

Reboot **without** init: the file is gone again. That is why auto-after-reboot needs step C.

#### Step C — Add boot init (`xmfh-init-script`)

Package path: `linkit/mypackages/application/xmfh-init-script`  
`.config`: `CONFIG_PACKAGE_xmfh-init-script=m` (APK, same as `ossdntest`)

Bringup script **keeps** 15.05 order, with only what 25.12.5 requires:

| 15.05 | Bringup |
| --- | --- |
| `test -x otdr_emu.sh \|\| exit 0` | **Skip** emu if missing; **do not exit** |
| Start `wifiwait.sh` | **Not started** (network left reset) |
| Start `imageflash.sh` | **Not started** (would write U-Boot / golden) |
| `gaugeSet`, flashers, `pwrmgt_daemonstchk` | Run only if the binary exists |
| GPIO export | Errors ignored (kernel 6.12 may already own the line) |
| Seed JSON + `boottimeproductinfo` | **Always** (if `ossdn-test` is installed) |

On the **build host**:

```bash
cd /home/amrit/XMF_H/openwrt/openwrt25.12.5_bringup/linkit
./scripts/feeds update mypackages
./scripts/feeds update packages
./scripts/feeds install libmraa
./scripts/feeds install -a -p mypackages
make defconfig
grep CONFIG_PACKAGE_xmfh-init-script .config
# expect: CONFIG_PACKAGE_xmfh-init-script=m

# Host tools + toolchain are not in git (staging_dir is gitignored).
make -j"$(nproc)" tools/install
make -j"$(nproc)" toolchain/install

grep -E '^CONFIG_PACKAGE_xmfh-init-script=' .config
# must be =m (or =y). If "# is not set", tools/toolchain reset .config — restore and make defconfig.

make package/xmfh-init-script/compile V=s
ls -l bin/packages/mipsel_24kc/mypackages/xmfh-init-script-*.apk
```

`libdeflate-gzip: No such file` while compiling lua host is **missing host tools**, not a bug in the init script. See [ossdn-test.md §7.5](./ossdn-test.md#75-libdeflate-gzip-no-such-file-while-compiling-a-package).

If you skip `feeds update packages`, you will see `Ignoring feed 'packages' - index missing` and `No feed for package 'libmraa' found`. `xmfh-init-script` still installs from mypackages. That is explained in [ossdn-test.md §7.4](./ossdn-test.md#74-feeds-install-warnings-libmraa--index-missing).

On the **device** (CLI APK must already be installed):

```sh
# from the PC, if SSH works:
# scp bin/packages/mipsel_24kc/mypackages/xmfh-init-script-*.apk root@BOARD:/tmp/

apk add --allow-untrusted /tmp/xmfh-init-script-*.apk
/etc/init.d/xmfh-init-script enable
ls -l /etc/rc.d/S99xmfh-init-script
/etc/init.d/xmfh-init-script enabled && echo enabled
```

`postinst` already runs `enable`. Check the symlink anyway.

No SSH: copy the APK on a USB stick the same way as [ossdn-test-install.md](./ossdn-test-install.md), then `apk add` on the UART shell.

#### Step D — Reboot and test auto `productinfo`

```sh
reboot
# after login (UART):
ls -l /tmp/product_info.json
cat /tmp/product_info.json
ossdn-test productinfo
logread | grep -i xmfh
# console should also show: XMF-H init script completed. Execution time was N seconds.
```

**PASS:** `/tmp/product_info.json` exists **without** typing `boottimeproductinfo`. `productinfo` is **not** 1201.

**FAIL — still 1201:**

```sh
which ossdn-test
/etc/init.d/xmfh-init-script enabled
ls -l /etc/rc.d/S99xmfh-init-script
/etc/init.d/xmfh-init-script start
ls -l /tmp/product_info.json
```

| Check | Fix |
| --- | --- |
| `ossdn-test` missing | Install `ossdntest` APK, reboot again |
| Init not enabled | `/etc/init.d/xmfh-init-script enable` then reboot |
| Init not installed | Step C |
| `start` creates the file, reboot does not | Enable failed; rc.d symlink missing |
| File exists, 1201 gone, fields empty | Success for this issue. Empty OTDR/MCU keys wait on emu / power daemon |

### 1.5 What this does **not** fix

- OTDR / emulator version in the JSON (`otdr_emu` still blocked).
- Freescale / MCU version (`powermgt-daemon` not in bringup yet).
- Battery `gaugeSet enable4` (skipped until that package exists).
- Wi-Fi / SSH (network left reset).
- I2C NACK on a **bare** LinkIt (N/A, same as 15.05).

Do not run `ossdn-test otdr_fw` / `ocm_fw` / `sfp_fw` / `fslrst` as part of this issue.

### 1.6 Files touched for this issue

| Path | Role |
| --- | --- |
| `linkit/mypackages/application/xmfh-init-script/` | New package |
| `linkit/.config` | `CONFIG_PACKAGE_xmfh-init-script=m` |
| `ossdn_test.c` | **unchanged** |

---

## Issue 2 — `apk add xmfh-init-script` runs U-Boot/golden `rm` and starts Wi-Fi delay

**Status:** fixed in package **`xmfh-init-script` 1.0.0-r2**.  
**Date:** 26 November 2026

### 2.1 Symptom

On the device (USB stick, no network):

```sh
apk --network=no add --allow-untrusted /mnt/usb/xmfh-init-script-1.0.0-r1.apk
```

Install **did** proceed (`Installing xmfh-init-script (1.0.0-r1)`). Then console showed:

```text
*****UBoot and GoldenImage*****
U-boot is upto date
rm: can't remove '/root/MCU/uboot.bin': No such file or directory
Golden Image is upto date
rm: can't remove '/root/MCU/lks7688_G.img': ...
Config is upto date
rm: can't remove '/root/MCU/update_ref_val.py': ...
*************END*************** 3 seconds.
Broadcast of WiFi delayed.
```

`imageflash.sh` ran **twice**. `wifiwait.sh` started (60s wait, then `wifi up radio0`).

The `WARNING: opening from cache https://downloads.openwrt.org/.../packages.adb` lines with `--network=no` are **harmless**. There is no online index; the local APK still installs with `--allow-untrusted`.

### 2.2 Cause

r1 installed **three** files under `/etc/init.d/`:

| File | Kind |
| --- | --- |
| `xmfh-init-script` | real `rc.common` (`START=99`) |
| `wifiwait.sh` | plain `#!/bin/sh` (not rc.common) |
| `imageflash.sh` | plain `#!/bin/sh` (not rc.common) |

`apk` post-install runs OpenWrt **`default_postinst`**, which **enables and starts every** `/etc/init.d/*` the package shipped. For a non-`rc.common` script, `enable` / `start` **runs the whole file**.

So `imageflash.sh` ran even though `start()` of `xmfh-init-script` never calls it. It always does `fw_setenv boot_success 1`, then `rm` of `/root/MCU/*` when env already says U-Boot/golden/config are current. If env had **not** matched, it would have tried `mtd write` of missing blobs.

`wifiwait.sh` then delayed and would enable the AP — against the network reset.

Not a bug in `ossdn_test.c`.

### 2.3 Steps to resolve

#### On the board **now** (r1 already installed)

Stop the Wi-Fi delay if it is still running, and remove the two scripts from init.d so reboot does not run them:

```sh
killall wifiwait.sh 2>/dev/null || true
rm -f /etc/init.d/wifiwait.sh /etc/init.d/imageflash.sh
ls /etc/init.d/xmfh-init-script /etc/rc.d/S99xmfh-init-script
```

`xmfh-init-script` itself stays. Reboot after that is safe for productinfo (issue 1). U-Boot env was only touched with `boot_success 1` if `fw_setenv` succeeded; the `rm` lines failed because those files were missing.

#### New APK (r2)

`wifiwait.sh` and `imageflash.sh` install to **`/usr/lib/xmfh/`**, not `/etc/init.d/`. Only `xmfh-init-script` is an init script. `start()` still does **not** call them.

On the **host**:

```bash
cd /home/amrit/XMF_H/openwrt/openwrt25.12.5_bringup/linkit
make package/xmfh-init-script/compile V=s
ls -l bin/packages/mipsel_24kc/mypackages/xmfh-init-script-1.0.0-r2.apk
```

On the **device** (USB stick):

```sh
apk --network=no add --allow-untrusted /mnt/usb/xmfh-init-script-1.0.0-r2.apk
ls /etc/init.d/wifiwait.sh /etc/init.d/imageflash.sh
# expect: not found
ls /usr/lib/xmfh/wifiwait.sh /usr/lib/xmfh/imageflash.sh
# expect: present
```

If apk says the package is already installed, use:

```sh
apk --network=no add --allow-untrusted --force-reinstall /mnt/usb/xmfh-init-script-1.0.0-r2.apk
```

or `apk del xmfh-init-script` then add r2. After r2, delete leftover `/etc/init.d/wifiwait.sh` and `imageflash.sh` if they remain from r1.

### 2.4 Verify

```sh
# apk add of r2 must NOT print UBoot and GoldenImage or Broadcast of WiFi delayed
ls /etc/init.d/xmfh-init-script
test ! -e /etc/init.d/imageflash.sh
test ! -e /etc/init.d/wifiwait.sh
/etc/init.d/xmfh-init-script enabled && echo enabled
```

Reboot: `productinfo` still as in issue 1. No golden/U-Boot flash. Wi-Fi stays as you left it.

### 2.5 What this does **not** fix

- Empty OTDR / MCU JSON fields (emu / power daemon still missing).
- Network / SSH (left reset; `wifiwait.sh` is on disk but not started).
- `packages.adb` cache warnings with `--network=no` — ignore them.

### 2.6 Files touched

| Path | Role |
| --- | --- |
| `xmfh-init-script/Makefile` | `PKG_RELEASE:=2`; scripts → `/usr/lib/xmfh/` |
| `files/xmfh-init-script` | helper paths updated |
| `ossdn_test.c` | **unchanged** |

---

## Issue 3 — (next)

Record the next `ossdn-test` runtime failure here: symptom, cause, steps, verify.

---

**OpenWrt:** v25.12.5 bringup  
**Related:** [ossdn-test.md §7](./ossdn-test.md#7-troubleshooting) · [test plan](./ossdn-test-testplan.md)
