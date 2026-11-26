
# openwrt25.12.5_bringup

> **Project Status:** Phase 4 — auto productinfo (`xmfh-init-script`)  
> **Target Platform:** MediaTek LinkIt Smart 7688  
> **OpenWrt Version:** v25.12.5

This project focuses on bringing up and adapting **OpenWrt v25.12.5** for the **MediaTek LinkIt Smart 7688** platform. The work is organized into development phases, with project documentation and automation maintained separately from the OpenWrt source tree.

**Current phase:** **4** — auto `productinfo` after reboot via `xmfh-init-script`.  
[→ Project phases](./docs/project-phases.md)

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Project Details](#2-project-details)
3. [Project Structure](#3-project-structure)
4. [Project Phases](#4-project-phases)
5. [Documentation Index](#5-documentation-index)
6. [Build Workspace](#6-build-workspace)
7. [Project Status](#7-project-status)

## 1. Project Overview

The goal of this project is to bring up OpenWrt v25.12.5 on the MediaTek LinkIt Smart 7688 and establish a maintainable platform for subsequent configuration, customization, firmware building, and validation.

The repository separates the project into three main areas:

- **Documentation** – project details, migration notes, configuration records, and development guides.
- **Scripts** – automation used to prepare the development environment and OpenWrt source tree.
- **OpenWrt source** – the source tree and build workspace for the LinkIt Smart 7688 target.

## 2. Project Details

| Item | Details |
|---|---|
| Project | `openwrt25.12.5_bringup` |
| Operating System | OpenWrt |
| OpenWrt Version | `v25.12.5` |
| Target Hardware | MediaTek LinkIt Smart 7688 |
| Target System | `ramips` |
| Subtarget | `mt76x8` |
| CPU Architecture | MIPS |
| Build Host | Ubuntu Linux 24.04.5 |
| Current Project Phase | 4 — ossdn-test modules (discussion) |

## 3. Project Structure

```text
openwrt25.12.5_bringup/
├── README.md
├── docs/
│   ├── migration.md
│   ├── initial_setup.md
│   ├── helloworld_hellocpp.md
│   ├── ossdn-test.md
│   ├── ossdn-test-install.md
│   ├── ossdn-test-source-changes.md
│   ├── ossdn-test-testplan.md
│   ├── ossdn-test-troubleshooting.md
│   ├── libwebsockets-disabled.md
│   ├── github-tags.md
│   └── project-phases.md
├── scripts/
│   └── initial_setup.sh
└── linkit/
    ├── mypackages/          # the only feed (src-link ../mypackages)
    │   ├── gcc14-legacy.mk
    │   ├── application/
    │   │   ├── helloworld/
    │   │   ├── hellocpp/
    │   │   ├── ossdn-test/
    │   │   └── xmfh-init-script/
    │   ├── libraries/
    │   └── additional_files/
    └── OpenWrt v25.12.5 source tree
```


## 4. Project Phases

Product work is four phases. Aim: 15.05 functionality on OpenWrt v25.12.5. Full write-up: [project-phases.md](./docs/project-phases.md).

| Phase | Description | Status | Documentation |
|---|---|---|---|
| Phase 1 – OpenWrt 25.12.5 setup | Host, `linkit/` tree, feeds, board config, first buildable image | Done | [Initial Setup](./docs/initial_setup.md) |
| Phase 2 – helloworld / hellocpp | C and C++ sample apps as APK modules | Done | [helloworld_hellocpp.md](./docs/helloworld_hellocpp.md) |
| Phase 3 – ossdn-test enable | Product CLI compiles and runs (musl); min libraries | Done | [ossdn-test.md](./docs/ossdn-test.md) |
| Phase 4 – ossdn-test full functionality | CLI depends on other 15.05 modules (init, power MCU, gauge, emu, …) | **Current** — `xmfh-init-script` for auto `productinfo` | [project-phases.md](./docs/project-phases.md), [troubleshooting](./docs/ossdn-test-troubleshooting.md) |

Network / Wi-Fi is left as reset (not phase 4).

### Current Phase: 4 — ossdn-test modules

The CLI from phase 3 starts. First companion: `xmfh-init-script` so `productinfo` works after reboot. Remaining modules (power daemon, gauge, emu) are later.

**[Open project phases →](./docs/project-phases.md)** · **[Troubleshooting issue 1 →](./docs/ossdn-test-troubleshooting.md)**

## 5. Documentation Index

Use this index to navigate project documentation.

| No. | Document | Description | Status |
|---|---|---|---|
| 1 | [Project phases](./docs/project-phases.md) | Phases 1–4; phase 4 started with auto `productinfo`. | **Current** |
| 2 | [Migration Details](./docs/migration.md) | Feature index and related notes. | Available |
| 3 | [Initial Setup](./docs/initial_setup.md) | Clone, setup script, OpenWrt source tree (phase 1). | Available |
| 3a | [helloworld / hellocpp](./docs/helloworld_hellocpp.md) | C and C++ APK modules (phase 2). | Available |
| 3b | [ossdn-test](./docs/ossdn-test.md) | Product CLI enable (phase 3). | Available |
| 3c | [ossdn-test source changes](./docs/ossdn-test-source-changes.md) | Every `.c` / `.h` / Makefile edit vs 15.05: problem, cause, fix. | Available |
| 3d | [ossdn-test test plan](./docs/ossdn-test-testplan.md) | What can PASS now, steps, dependencies; what cannot yet and why. | Available |
| 3e | [ossdn-test troubleshooting](./docs/ossdn-test-troubleshooting.md) | One issue at a time; issue 1 = productinfo 1201 / auto after reboot. | Available |
| 4 | Target Configuration | LinkIt Smart 7688 target and device configuration. | Planned |
| 5 | Kernel Configuration | Kernel options, drivers, and kernel customization. | Planned |
| 6 | Package Development | Custom OpenWrt package development and integration. | Planned |
| 7 | [Wireless and network](./docs/wireless_network.md) | Left as reset (not phase 4). | Left as-is |
| 8 | Firmware Build | Firmware configuration and build procedure. | Planned |
| 9 | Firmware Deployment | Firmware deployment and recovery procedure. | Planned |
| 10 | Testing | Platform and functional validation. | Planned |

## 6. Build Workspace

The OpenWrt source tree is in `linkit/`. Board config (`.config`) and pinned `feeds.conf` are tracked. Build caches (`bin/`, `build_dir/`, `dl/`, `staging_dir/`, `feeds/`) are gitignored.

After **cloning this repository** (setup already done on the original machine):

```bash
cd linkit
./scripts/feeds update -a
./scripts/feeds install -a
make -j"$(nproc)"
```

After **first-time setup** on an empty tree, see [Initial Setup](./docs/initial_setup.md), then:

```bash
cd linkit
make menuconfig
```

## 7. Project Status

| Area | Status |
|---|---|
| Repository and documentation structure | Available |
| Initial setup automation | Available |
| OpenWrt v25.12.5 source preparation | Available through setup script |
| Migration / phases 1–3 | Done (setup, hellos, ossdn-test enable) |
| Phase 4 ossdn-test modules | **Current** — auto productinfo via `xmfh-init-script` |
| Target-specific configuration | Planned / subject to migration results |
| Build integration | Planned |
| Deployment and validation | Planned |

Update this file when the active phase changes, and maintain detailed implementation notes in the corresponding document under `docs/`.

---

**Project:** `openwrt25.12.5_bringup`  
**Target:** MediaTek LinkIt Smart 7688  
**Author:** Amritpal Singh
