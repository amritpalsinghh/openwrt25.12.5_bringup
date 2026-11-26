# ossdn-test — test plan (bringup)

[← Back to Main README](../README.md) · [Build / run](./ossdn-test.md) · [Install APK](./ossdn-test-install.md) · [Source changes](./ossdn-test-source-changes.md) · [Troubleshooting log](./ossdn-test-troubleshooting.md) · [Phase 4](./project-phases.md)

This is the **test plan for `ossdn-test` on OpenWrt v25.12.5 bringup**. It says what can succeed **now**, what you need, how to run it, and what **cannot** be tested yet (and why).

**Tree:** `openwrt25.12.5_bringup`  
**On device:** `/bin/ossdn-test` (APK `ossdntest`; current config `=m`)  
**Console:** ttyS2 **57600**  
**LAN (if up):** `192.168.100.1`

Network / Wi-Fi was **reset**. Tests that need SSH over STA/AP may need the **serial console** instead.

---

## 1. How to read a result

| Verdict | Meaning |
| --- | --- |
| **PASS** | Command did what 15.05 would do on this hardware |
| **PASS (expected error)** | CLI ran; the error is the same 15.05 behaviour (missing file, NACK on bare board) |
| **N/A** | Hardware not present (bare LinkIt). Not a firmware fail |
| **NOT YET** | CLI can start the command, but a **companion package / file / daemon** is not on this image |
| **BLOCKED** | Cannot succeed until original C sources exist (`otdr_emu`, `xmfhwxx`) |
| **DO NOT RUN** | Writes instrument / MCU firmware or hard-resets the Freescale |

Classify a failure before changing C:

1. **missing chip** (bare module) → N/A  
2. **missing file** (`/tmp/product_info.json`, MCU blob) → init / firmware-files  
3. **missing daemon** (power MCU / emu) → phase 4 package  
4. **CLI not found / not musl** → install / compile (phase 3)

---

## 2. Two boards

| Board | What “success” means |
| --- | --- |
| **Bare LinkIt Smart 7688** | CLI starts, usage prints, `ldd` is musl, `productinfo` 1201 until you run `boottimeproductinfo`. I2C/LED/OTDR/OCM **NACK is N/A**. |
| **Packed XMF-H** (LinkIt + instruments) | Same CLI tests, plus I2C / LED / battery / IO expander can **PASS** without extra packages. OTDR / Freescale / boot-init still need phase 4 modules. |

Record which board you used.

---

## 3. What is on this image (dependencies)

### 3.1 Present now (phase 3)

| Item | Role |
| --- | --- |
| `ossdntest` APK | `/bin/ossdn-test` after [install](./ossdn-test-install.md) |
| `xmfh-init-script` APK | `/etc/init.d/xmfh-init-script` (`=m`); enable then reboot for auto `productinfo` |
| `libmraa`, `liblog-maker`, `libocminterface`, `libotdrinterface`, `libhweventcallback`, `libi2cinterface`, `libledinterface`, `libbatterymanagement` | `=y` in the image |
| Kernel I2C / GPIO (6.12) | mraa talks to the bus |

### 3.2 Not on this image (later phase 4)

| Missing | What tests need it |
| --- | --- |
| `powermgt-daemon` / `pwrmgt_daemonstchk` | `powermgt`, `fslslavechk`, `fslver`, MCU fields in JSON |
| `gaugeSet` + `firmware-files` | Battery enable path (`gaugeSet enable4`) — not an `ossdn-test` subcommand, but 15.05 boot |
| Flashers + `/root/MCU` `/root/POWR607` | Optional MCU / POWR607 flash at boot |
| `otdr_emu` | `otdr`, `emu_*`, OTDR fields in product JSON, 15.05 init **exits** without it |
| `xmfhwxx` | Separate boot daemon |
| Network stack | SSH over Wi-Fi; `wifiwait.sh`; `sw3_check` may call `wifi_mode` |

### 3.3 Safety — do not run unless you intend to write firmware

```sh
# DO NOT RUN
ossdn-test otdr_fw <file>
ossdn-test ocm_fw <file>
ossdn-test sfp_fw <file>
ossdn-test fslrst 0
ossdn-test fslrst 1
ossdn-test boot_otdr_fw
```

`/bin/shutdown` on the product is **xmfhshutdown** (halt). Use `reboot` for a normal restart. That binary is **not** in this bringup feed.

---

## 4. Tests that can succeed now

Do these in order. Host tests first, then board.

### 4.1 Host — APK exists and is musl (no board)

**Dependency:** `make package/ossdn-test/compile` already done.

```sh
cd /home/amrit/XMF_H/openwrt/openwrt25.12.5_bringup/linkit
ls -l bin/packages/mipsel_24kc/mypackages/ossdntest-*.apk
file bin/packages/mipsel_24kc/mypackages/ossdntest-*.apk
```

**PASS:** APK present. (The APK is an archive; the musl check is on the device `ldd` / `file` of `/bin/ossdn-test` after install.)

If the APK is missing: [ossdn-test.md §5](./ossdn-test.md#5-compile).

---

### 4.2 Board — binary installed, musl, libraries resolve

**Dependency:** APK installed ([ossdn-test-install.md](./ossdn-test-install.md)). Serial console if SSH is down.

```sh
which ossdn-test
ls -l /bin/ossdn-test
file /bin/ossdn-test
# expect: MIPS, interpreter ld-musl-mipsel.so.1   (not ld-uClibc)

ldd /bin/ossdn-test
# expect: libmraa, libotdrinterface, libocminterface, libi2cinterface,
#         libledinterface, libbatterymanagement, libhweventcallback, liblog-maker
#         all "found" — no "not found"
```

**PASS:** interpreter is musl; every listed `.so` resolves.  
**FAIL:** `not found` → APK not installed. Missing `.so` → image was not built with libs `=y`; copy those APKs too.

---

### 4.3 Board — CLI starts (usage)

**Dependency:** §4.2 PASS.

```sh
ossdn-test
```

**PASS:** prints the usage banner (`ossdn-test app Usage`, OTDR/OCM/SFP/LED/… lines) and exits 0. Does **not** crash, does **not** say `not found`.

This is the phase 3 “CLI works” test. It does not talk to hardware.

---

### 4.4 Board — `productinfo` 1201 without cache (expected)

**Dependency:** §4.3 PASS. **No** `xmfh-init-script` on this image.

```sh
ls -l /tmp/product_info.json
# expect: No such file or directory

ossdn-test productinfo
```

**PASS (expected error):**

```
productinfo error: 1201
and msg:
```

Empty `msg:` is 15.05: `fopen` failed, buffer stayed zeros. See [ossdn-test.md §7.1](./ossdn-test.md#71-ossdn-test-productinfo--productinfo-error-1201-and-empty-msg).

**FAIL:** segfault, or a different unexplained error with the file present.

---

### 4.5 Board — `boottimeproductinfo` then `productinfo`

**Dependency:** §4.3 PASS. Packed XMF-H for filled fields; bare LinkIt still creates or tries to create the file.

```sh
ossdn-test boottimeproductinfo
ls -l /tmp/product_info.json
cat /tmp/product_info.json
ossdn-test productinfo
```

| Board | PASS |
| --- | --- |
| Packed XMF-H | File exists; `productinfo` prints JSON (some keys may be blank if a slave NACKed) |
| Bare LinkIt | Command **runs** (does not crash). File may be missing or sparse; I2C errors **N/A**. After that, `productinfo` is 1201 if the file was not written |

`/tmp` is tmpfs: **reboot clears the file**. Without init, you must run `boottimeproductinfo` again after every boot. That is **NOT YET** auto; it is a successful **manual** test.

---

### 4.6 Board — init-order commands by hand (hardware)

**Dependency:** packed XMF-H for PASS; bare LinkIt → N/A on NACK. No extra packages.

Run in 15.05 boot order (skip Wi-Fi):

```sh
ossdn-test hwrev; echo rc=$?
ossdn-test sw3_check; echo rc=$?
ossdn-test IO_EXP2 kl02; echo rc=$?
ossdn-test fslslavechk; echo rc=$?
ossdn-test IO_EXP2 powr607; echo rc=$?
ossdn-test op_sw_en; echo rc=$?
ossdn-test led 1; echo rc=$?
ossdn-test boottimeproductinfo; echo rc=$?
ossdn-test productinfo
ossdn-test power_off_otdr; echo rc=$?
```

| Command | Needs | PASS now? |
| --- | --- | --- |
| `hwrev` | I2C / EEPROM (or flag file) | Packed: yes. Bare: N/A |
| `sw3_check` | IO expander 0x26 (hw 1.1); may call `wifi_mode` | Packed hw 1.1: yes. Network reset: Wi-Fi side **not** a CLI fail |
| `IO_EXP2 kl02` / `powr607` | mraa I2C 0x26; hw 1.1 file | Packed hw 1.1: yes. Else prints status / skips |
| `fslslavechk` | Freescale / PwrMgtMCU via UART daemon | **Often NOT YET** — no `powermgt-daemon`. Command runs; slave fail is missing daemon or chip |
| `op_sw_en` | I2C / expander | Packed: often yes |
| `led 1` | LED I2C | Packed: yes. Bare: N/A |
| `power_off_otdr` | OTDR power path | Packed: may PASS even without emu. Full OTDR scan is NOT YET |

Do **not** treat a NACK on a bare module as a failed port.

---

### 4.7 Board — safe instrument CLIs (packed unit only)

**Dependency:** packed XMF-H. OTDR **scan** still needs `otdr_emu` for 15.05-shaped results.

```sh
# LED (I2C)
ossdn-test led 1
ossdn-test led stop_animation
ossdn-test mirror 0

# Battery (I2C gauge)
ossdn-test batterymanagement 1 9
ossdn-test batterymanagement 1 3
ossdn-test batterymanagement 1 5

# SFP (if cage populated)
ossdn-test sfp 1

# OCM identity (if OCM present; no emu required for some get-version APIs)
ossdn-test ocm 1 1
ossdn-test ocm 1 5
ossdn-test ocm 1 6

# OCC / OSA — hardware
ossdn-test occ
# ossdn-test osa_cal     # read only; skip set/reset unless you mean to write cal
```

**PASS:** API status 0 / printed values.  
**N/A:** NACK, no module.  
**NOT YET:** hangs waiting on emu socket `/tmp/ii_vi_otdr_emu` → missing `otdr_emu`.

Debug log (safe):

```sh
ossdn-test enable_hw_debug on
ossdn-test enable_hw_debug off
```

---

### 4.8 Optional — I2C stress (packed, short loop)

**Dependency:** packed XMF-H. Do **not** use `-1` (infinite) unless you can Ctrl+C.

```sh
ossdn-test i2cstresstest fsl 10
ossdn-test i2cstresstest linkit 10
ossdn-test i2cstresstest mcu 10
```

**PASS:** 10 iterations, no hang.  
**NOT YET / N/A:** MCU/FSL paths without `powermgt-daemon` or chips.

---

### 4.9 Board — auto `productinfo` after reboot

**Dependency:** `ossdntest` APK **and** `xmfh-init-script` APK installed; init **enabled**. Steps: [troubleshooting issue 1](./ossdn-test-troubleshooting.md#issue-1--productinfo-error-1201-empty-msg-and-no-auto-file-after-reboot).

```sh
/etc/init.d/xmfh-init-script enabled && echo enabled
ls -l /etc/rc.d/S99xmfh-init-script
reboot
# after login — do not run boottimeproductinfo by hand:
ls -l /tmp/product_info.json
ossdn-test productinfo
```

**PASS:** file exists; `productinfo` is **not** 1201. JSON keys may still be empty (no emu / no power daemon / bare LinkIt).  
**FAIL:** still 1201 → CLI or init not installed/enabled. Do not edit `ossdn_test.c`.

---

## 5. Tests that cannot succeed yet (and why)

These commands may **start**. They cannot match 15.05 product behaviour on **this** bringup image.

| Test | Command / check | Why not yet | Unblocks when |
| --- | --- | --- | --- |
| Full 15.05 boot (emu + gauge + Wi-Fi + golden) | Console line plus those daemons running | Init now **skips** missing companions so `productinfo` can run | later packages; emu **blocked** |
| GPIO 0 / 14 / 17 as 15.05 | `ls /sys/class/gpio/gpio14` | Init tries export; kernel **6.12** sysfs may already own the line | hardware proof on 6.12 |
| `fslslavechk` / `fslver` / `powermgt` like 15.05 | `ossdn-test fslslavechk` `fslver` `powermgt` | No `pwrmgt_daemonstchk` / UART daemon | `powermgt-daemon` |
| MCU version in product JSON | `cat /tmp/product_info.json` | Daemon + `boottimeproductinfo` at boot | init + power daemon |
| `gaugeSet enable4` | `/root/fuelgauge_flags/gaugeSet enable4` | Package not in bringup | `gaugeSet` + `firmware-files` |
| OTDR version / scan / live | `ossdn-test otdr normal_scan 0 1 1 1` (and 6, 18, …) | 15.05 talks to **`otdr_emu`** over Unix socket; init **exits** if emu missing | **BLOCKED:** uClibc binary, **no musl C sources** |
| `emu_start` / `emu_stop` / `emu_reinit` | `ossdn-test emu_start` | Same emu | Same block |
| OTDR fields in product JSON | after `boottimeproductinfo` | Emu / OTDR UART | Same block |
| `xmfhwxx` boot daemon | `ps \| grep xmfhwxx` | Not in image; uClibc | **BLOCKED** until sources |
| Boot flashers | `/root/MCU/*.bin` present and flashed | Blobs + flasher packages not in bringup | flashers + firmware-files (optional; 15.05 skips if missing) |
| Web CGI MCU upgrade | LuCI / `test.py` | `mtk-linkit-webui` product tree not installed | webui slice (not required for CLI compile) |
| SSH to board over Wi-Fi | `ssh root@…` | Network stack **left reset** | Separate network work; use UART for CLI tests |
| `wifiwait.sh` / AP delay | boot Wi-Fi | Out of scope | Not phase 4 |
| `eventTest` as 15.05 | `ossdn-test eventTest` (SFP / power / MF buttons) | Needs GPIO export + hardware; power-button GPIO 14 from init | init + packed unit. CLI can still be started to see register errors |

### 5.1 OTDR — why “command runs” is not a pass

```sh
# This is NOT a pass/fail for phase 3:
ossdn-test otdr normal_scan 0 1 1 1
```

The binary will **invoke** the OTDR library. Without `otdr_emu` (and often without OTDR power from init), you get socket/timeout/UART errors. That proves the CLI **entry point** exists, not 15.05 OTDR functionality. Do not iterate on `ossdn_test.c` to fake the emu.

---

## 6. Suggested session (packed XMF-H, UART)

1. Install `ossdntest` APK if needed ([install](./ossdn-test-install.md)).  
2. §4.2 `file` / `ldd`.  
3. §4.3 `ossdn-test` (usage).  
4. §4.4 `productinfo` → **1201** (before init).  
5. §4.5 `boottimeproductinfo` then `productinfo`.  
6. Install `xmfh-init-script`, enable, **reboot**, §4.9 auto `productinfo`.  
7. §4.6 / §4.7 LED + battery on packed hardware.  
8. Stop. Do **not** run §3.3 firmware commands.  
9. Record OTDR / `powermgt` / `fslslavechk` as **NOT YET** or **BLOCKED**, not as a compile fail.

Bare LinkIt: §4.9 can still PASS (file exists, not 1201) with empty JSON fields.

---

## 7. Map to project phases

| Phase | Test plan |
| --- | --- |
| **3** (done) | §4.1–§4.5 must PASS. §4.6–§4.7 PASS on packed hardware, N/A on bare |
| **4** (started) | §4.9 auto `productinfo` after `xmfh-init-script`. Remaining §5 as companions land |
| Blocked | `otdr_emu`, `xmfhwxx` until original sources exist |

Full 15.05 product suite (init, GPIO, gauge, web) is documented for the **old migrated tree** in `docs/TESTCASES_PRODUCT_FUNCTIONAL.md`. That suite assumes packages this bringup **does not** have. Use **this** file for bringup; use that file after phase 4 modules exist.

---

**OpenWrt:** v25.12.5 bringup  
**Legacy:** OpenWrt 15.05 XMF-H  
**Status:** phase 3 CLI tests runnable; phase 4 companion tests not yet
