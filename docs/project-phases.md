# Project phases — OpenWrt 25.12.5 bring-up

[← Back to Main README](../README.md)

**Aim:** bring **15.05 product functionality** onto **OpenWrt v25.12.5** (LinkIt Smart 7688). Change only what the new system requires (musl, GCC 14, apk, kernel 6.12). Legacy tree `openwrt15.05` stays read-only.

**Work tree:** `openwrt25.12.5_bringup`  
**Current phase:** **4** — make `ossdn-test` product behaviour work by bringing the modules it depends on. First slice: auto `productinfo` after reboot (`xmfh-init-script`).

Network / Wi-Fi was tried, then **reset**. It is **not** part of phase 4.

---

## Phase map

| Phase | What | Status | Documentation |
| --- | --- | --- | --- |
| **1** | Setup OpenWrt **v25.12.5** so the tree builds | Done | [initial_setup.md](./initial_setup.md), tags `v0.0.1` / `v0.0.2` |
| **2** | **helloworld** and **hellocpp** as modules | Done | [helloworld_hellocpp.md](./helloworld_hellocpp.md), tag `v0.0.3` |
| **3** | **ossdn-test** enable (CLI compiles and runs) | Done | [ossdn-test.md](./ossdn-test.md), tag `v0.0.4`; docs `v0.0.5` |
| **4** | **ossdn-test full functionality** via other 15.05 modules | **Current** — init slice started | this file; [troubleshooting](./ossdn-test-troubleshooting.md) |

How to tag later: [github-tags.md](./github-tags.md).

---

## Phase 1 — Setup of OpenWrt 25.12.5

Host Ubuntu, clone bring-up, run `scripts/initial_setup.sh`, OpenWrt lives in `linkit/`. Target `ramips` / `mt76x8` / LinkIt Smart 7688. Feeds pinned; custom feed later `src-link mypackages ../mypackages` → `linkit/mypackages`. Console **ttyS2 57600**. Image is apk/musl/GCC 14, not 15.05 ipk/uClibc/GCC 4.8.

Outcome: a tree that can `make` firmware. No product CLI yet.

---

## Phase 2 — helloworld and hellocpp modules

Simple C and C++ userspace programs under `linkit/mypackages/application/`. Built as **APK modules** (`=m`): not required inside squashfs; install with `apk add` after flash. Proved the custom feed, `gcc14-legacy.mk` layout, and module vs image (`=y` / `=m`) workflow.

Outcome: known-good path to add a package to `mypackages` and run it on the board.

---

## Phase 3 — ossdn-test enable

The 15.05 product CLI at `linkit/mypackages/application/ossdn-test` (`PKG_NAME` `ossdntest`, device `/bin/ossdn-test`).

- Minimum libraries in the same feed (log, OCM, OTDR, I2C, LED, battery, events) plus **libmraa** from the packages feed.
- CLI source (`ossdn_test.c`) **unchanged**. Libraries got only musl/GCC 14 fixes. Full list (problem, cause, fix): [ossdn-test-source-changes.md](./ossdn-test-source-changes.md).
- Current config: `ossdntest=m` (APK); support libraries `=y` in the image.
- 15.05 `libwebsockets` in `additional_files` is **disabled** (clash with packages 4.4.1). Not needed for this CLI.
- `productinfo` error **1201** with empty `msg:` means `/tmp/product_info.json` is missing — that file is written by `boottimeproductinfo` / boot init, not by `productinfo` itself.
- Install when not in the image: [ossdn-test-install.md](./ossdn-test-install.md).

Outcome: the CLI **starts** on musl. That is **layer 1**. It is not yet 15.05 product behaviour.

---

## Phase 4 — ossdn-test functionality that depends on other modules

**Status:** first slice in the tree. Package `xmfh-init-script` (`=m`) seeds `/tmp/product_info.json` and runs `boottimeproductinfo` at boot. Missing emu / gauge / Wi-Fi / imageflash are **skipped** so this still completes. Network stack stays reset.

### 4.1 Aim of this phase

On 15.05, `ossdn-test` is not a self-contained product. Boot init **calls** it, and many subcommands expect **other programs, files, and chips**. Phase 4 is: bring those 15.05 pieces onto 25.12.5 **one module at a time**, so the same commands mean the same thing as on the old image.

Do not enlarge `ossdn_test.c`. Do not invent musl rebuilds of binaries that have no C sources.

### 4.2 Two kinds of “works”

**A. CLI + libraries (phase 3 — done)**  
Commands such as `led`, `hwrev`, `IO_EXP2`, `batterymanagement`, and `productinfo` after `boottimeproductinfo` talk to I2C / mraa / JSON. On a **packed XMF-H** they can succeed without the OTDR emulator or web UI. On a **bare LinkIt** they NACK. That is also 15.05.

**B. Product behaviour (phase 4 — this work)**  
At boot, 15.05 `xmfh-init-script` (START=99) does the following. Until those companions exist on 25.12.5, many commands will run and still return hardware or missing-file errors. That is expected, not a failed CLI port.

### 4.3 What 15.05 boot actually does with ossdn-test

From `openwrt15.05/mypackages/application/xmfh-init-script` (read-only reference):

1. `ossdn-test hwrev`
2. GPIO sysfs **0** (out, then 1)
3. `ossdn-test sw3_check`
4. `ossdn-test IO_EXP2 kl02`
5. optional PwrMgtMCU flasher if `/root/MCU/*.bin` exists
6. `ossdn-test fslslavechk`
7. `ossdn-test IO_EXP2 powr607` then optional POWR607 flasher if blobs exist
8. `ossdn-test op_sw_en`
9. maybe `ossdn-test led stop_animation` if `/root/LED_STOP_ANIMATION` exists
10. start **`pwrmgt_daemonstchk`**
11. GPIO **14** (power button, in, both edges) and **17** (out, 1)
12. start **`otdr_emu`** (init **exits** if the emu script is missing)
13. seed `/tmp/product_info.json`
14. **`gaugeSet enable4`**
15. **`wifiwait.sh`** (delay AP — **out of scope**; network left reset)
16. **`ossdn-test boottimeproductinfo`** then **`power_off_otdr`**
17. flag files `/tmp/otdr_init_progress`, `/tmp/ocm_init_progress`
18. **`imageflash.sh`** (U-Boot `boot_success` / golden)
19. `gaugeSet charger configCheck`

So “all functionality” is **CLI + those companions + real hardware**.

`ossdn-test` usage on 15.05 also includes: `otdr` / `ocm` / `occ` / `sfp` / `led` / `batterymanagement` / `powermgt` / `eventTest` / `IO_EXP2` and the init-only cmds above.

### 4.4 Sensible order (one module at a time)

Copy from 15.05 or from the already-migrated feed `openwrt25_12_5/mypackages` into **`linkit/mypackages`**, enable, test on the board, then the next package. Do not dump the whole feed in one go. `openwrt25_12_5` already **compiled** most of these (26 APKs); bringup has **not** replayed them.

| Step | Why | What ossdn-test gains |
| --- | --- | --- |
| 1. Keep the CLI as in phase 3 | Proven musl binary | Manual commands |
| 2. `xmfh-init-script` (skip `wifiwait.sh` / emu exit / imageflash) | **Done in tree** (`=m`). 15.05 never ran `boottimeproductinfo` by hand | Auto `productinfo` after reboot; GPIO / hwrev / op_sw_en attempted |
| 3. `powermgt-daemon` + check script | Init starts it; Freescale / `powermgt` need the UART daemon | `fslslavechk`, MCU version in product JSON |
| 4. `gaugeSet` + `firmware-files` layout | Init runs `gaugeSet enable4` | Battery path matches 15.05 |
| 5. Flashers + `/root/MCU` `/root/POWR607` blobs | Init only flashes if files exist | Same skip-if-missing as 15.05 |
| 6. `otdr_emu` | 15.05 init exited without it; bringup **skips** so productinfo still runs | **Blocked:** uClibc binary, **no musl C sources**. Do not invent a rewrite |
| 7. `xmfhwxx` | Separate boot daemon `/usr/bin/xmfhwxx` | Same block: uClibc, no sources |
| 8. `mtk-linkit` / webui / `test.py` | CGI used for some MCU upgrades | Product web; not required for CLI compile |
| 9. Network | `wifiwait.sh` only delays AP | Separate track; left reset |

### 4.5 What you can test now

After `xmfh-init-script` is installed and enabled, reboot once and `ossdn-test productinfo` should not return 1201. On a **packed** unit you can still run the same order by hand:

`hwrev` → `sw3_check` → `IO_EXP2` → `fslslavechk` → `op_sw_en` → `led` → `boottimeproductinfo` → `productinfo` → `power_off_otdr`

Then instrument CLIs: `otdr` / `ocm` / `sfp` / `batterymanagement` / `powermgt` / `eventTest`.

If a command fails, classify it:

- **missing chip** (bare module) → N/A, not a firmware fail
- **missing daemon** (power MCU / emu) → next 15.05 package
- **missing file** (`/tmp/product_info.json`, MCU blob) → init or firmware-files

Do **not** run `otdr_fw` / `ocm_fw` / `sfp_fw` / `fslrst` unless you intend to write instrument firmware.

### 4.6 Hard rules for 15.05 → 25.12.5

- Keep 15.05 logic. Change C only for musl / GCC 14 (already done for this CLI).
- GPIO sysfs in init may differ on kernel **6.12** — prove on hardware before rewriting.
- `otdr_emu` / `xmfhwxx` cannot be made to **run** by packing the old binaries. Full OTDR boot parity waits on sources.
- `extra_provides` uClibc names in apk are **not** a runtime fix.
- Do not convert init to procd unless asked (15.05 uses `rc.common`).
- Do not rebuild firmware or flash unless asked.
- Network / mesh stays as the user reset it.

### 4.7 Practical next choice (when implementation starts)

Smallest 15.05-shaped step: **`xmfh-init-script`** (optionally without `wifiwait.sh`) plus **`powermgt-daemon`**, so boot does what 15.05 did for `ossdn-test` **before** the emu.

OTDR-complete behaviour waits on emu sources.

Pick one slice when ready: **init-only**, **power MCU**, **gauge**, or **OTDR** (blocked until sources).

### 4.8 Related notes (phase 3, still valid)

| Topic | Doc |
| --- | --- |
| Build / `=m` vs `=y` | [ossdn-test.md](./ossdn-test.md) |
| `.c` / `.h` vs 15.05 | [ossdn-test-source-changes.md](./ossdn-test-source-changes.md) |
| Test plan (can / cannot yet) | [ossdn-test-testplan.md](./ossdn-test-testplan.md) |
| Troubleshooting log (issue 1 = productinfo 1201) | [ossdn-test-troubleshooting.md](./ossdn-test-troubleshooting.md) |
| APK install (SSH / USB) | [ossdn-test-install.md](./ossdn-test-install.md) |
| `productinfo` 1201 | [ossdn-test.md §7](./ossdn-test.md#7-troubleshooting) |
| Old libwebsockets Makefile | [libwebsockets-disabled.md](./libwebsockets-disabled.md) |

---

**OpenWrt:** v25.12.5  
**Legacy:** OpenWrt 15.05 XMF-H  
**Phase 4:** first slice 24 November 2026 — `xmfh-init-script` for auto `productinfo`; remaining companions not copied
