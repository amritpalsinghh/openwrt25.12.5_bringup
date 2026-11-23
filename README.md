
# openwrt25.12.5_bringup

> **Project Status:** Migration Phase  
> **Target Platform:** MediaTek LinkIt Smart 7688  
> **OpenWrt Version:** v25.12.5

This project focuses on bringing up and adapting **OpenWrt v25.12.5** for the **MediaTek LinkIt Smart 7688** platform. The work is organized into development phases, with project documentation and automation maintained separately from the OpenWrt source tree.

**Current phase:** The project is currently in the **Migration Phase**.  
[→ Migration Details](./docs/migration.md)

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
| Current Project Phase | Migration |

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
│   ├── libwebsockets-disabled.md
│   └── github-tags.md
├── scripts/
│   └── initial_setup.sh
└── linkit/
    ├── mypackages/          # the only feed (src-link ../mypackages)
    │   ├── gcc14-legacy.mk
    │   ├── application/
    │   │   ├── helloworld/
    │   │   ├── hellocpp/
    │   │   └── ossdn-test/
    │   ├── libraries/
    │   └── additional_files/
    └── OpenWrt v25.12.5 source tree
```


## 4. Project Phases

The project is organized into the following phases. The migration phase is the current active phase; subsequent phases will be updated as work progresses.

| Phase | Description | Status | Documentation |
|---|---|---|---|
| Phase 1 – Initial Setup | Prepare the repository layout, host build environment, OpenWrt source tree, feeds, and initial target configuration. | Available | [Initial Setup](./docs/initial_setup.md) |
| Phase 2 – Migration | Perform and document the migration work required for the OpenWrt v25.12.5 bring-up. | **In Progress – Current Phase** | [Migration Details](./docs/migration.md) |
| Phase 3 – Target Configuration | Review and complete target-specific configuration for the LinkIt Smart 7688. | Planned | To be documented |
| Phase 4 – Build Integration | Configure and build the required OpenWrt image and integrate required components. | Planned | To be documented |
| Phase 5 – Deployment and Validation | Deploy the generated image and validate boot, connectivity, and platform functionality. | Planned | To be documented |

### Current Phase: Migration

The project is currently focused on migration activities.

Refer to the migration document for the migration scope, steps, decisions, issues, and progress:

**[Open Migration Details →](./docs/migration.md)**

## 5. Documentation Index

Use this index to navigate project documentation.

| No. | Document | Description | Status |
|---|---|---|---|
| 1 | [Migration Details](./docs/migration.md) | Current migration phase, migration activities, progress, and related notes. | In Progress |
| 2 | [Initial Setup](./docs/initial_setup.md) | Clone the parent repository, run the setup script, prepare the OpenWrt source tree, and continue to the build workspace. | Available |
| 2a | [helloworld / hellocpp](./docs/helloworld_hellocpp.md) | Simple C and C++ apps: build into the image (`=y`) or as APK modules (`=m`). | Available |
| 3 | Target Configuration | LinkIt Smart 7688 target and device configuration. | Planned |
| 4 | Kernel Configuration | Kernel options, drivers, and kernel customization. | Planned |
| 5 | Package Development | Custom OpenWrt package development and integration. | Planned |
| 6 | [Wireless and network](./docs/wireless_network.md) | Bake AP+STA and LAN 192.168.100.1 into the image (no hand-run UCI). | Available |
| 7 | Network Configuration | Extra interface notes beyond [wireless_network.md](./docs/wireless_network.md). | Planned |
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
| Migration | **In Progress** |
| Target-specific configuration | Planned / subject to migration results |
| Build integration | Planned |
| Deployment and validation | Planned |

Update this file when the active phase changes, and maintain detailed implementation notes in the corresponding document under `docs/`.

---

**Project:** `openwrt25.12.5_bringup`  
**Target:** MediaTek LinkIt Smart 7688  
**Author:** Amritpal Singh
