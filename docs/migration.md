[← Back to Main README](../README.md.md)
# OpenWrt – LinkIt Smart 7688

OpenWrt v25.12.5 migration, customization and development for the MediaTek LinkIt Smart 7688 platform.

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Platform Information](#2-platform-information)
3. [Migration Workflow](#3-migration-workflow)
4. [Feature Index](#4-feature-index)
5. [Status Definitions](#5-status-definitions)
6. [Documentation Guidelines](#6-documentation-guidelines)

---

## 1. Project Overview

This project focuses on migrating and customizing OpenWrt for the MediaTek LinkIt Smart 7688 platform.

It covers environment setup, system configuration, package development, networking, firmware generation, deployment and hardware validation.

## 2. Details

### 2.1 Platform Information
| Parameter | Details |
|---|---|
| Operating System | OpenWrt |
| Version | v25.12.5 |
| Hardware | MediaTek LinkIt Smart 7688 |
| Architecture | MIPS |
| Development Host | Ubuntu Linux |

### 2.2 . Project Structure at Start

```text
openwrt25.12.5_bringup/
├── README.md
├── docs/
│   ├── migration.md
│   └── initial_setup.md
├── scripts/
│   └── initial_setup.sh
└── .gitignore
    
```
## 3. Migration Workflow

The overall development and migration process follows these phases:

1. **Initial Setup** – Prepare the development environment and OpenWrt source.


## 4. Feature Index

Individual feature documentation, implementation details and usage instructions are maintained under `linkit/docs/`.

| Feature | Description | Documentation | Status |
|---|---|---|---|
| Initial Setup | Development environment and source preparation | [initial_setup.md](./linkit/docs/initial_setup.md) | Available |
| Configuration | OpenWrt target and system configuration | [target_configuration.md](./linkit/docs/target_configuration.md) | Planned |
| Package Development | Custom package development and integration | [package_development.md](./linkit/docs/package_development.md) | Planned |
| Wi-Fi Configuration | Wireless interface configuration | [wifi_configuration.md](./linkit/docs/wifi_configuration.md) | Planned |
| Network Configuration | Interfaces, DHCP and SSH configuration | [network_configuration.md](./linkit/docs/network_configuration.md) | Planned |
| Firmware Build | OpenWrt firmware compilation | [firmware_build.md](./linkit/docs/firmware_build.md) | Planned |
| Firmware Deployment | Firmware flashing and deployment | [firmware_deployment.md](./linkit/docs/firmware_deployment.md) | Planned |
| Testing & Validation | Hardware and functional validation | [testing.md](./linkit/docs/testing.md) | Planned |

## 5. Status Definitions

| Status | Description |
|---|---|
| Available | Implementation and documentation are available. |
| In Progress | Feature development is ongoing. |
| Planned | Feature is identified for future development. |
| Completed | Implementation and validation are complete. |

## 6. Documentation Guidelines

- Maintain individual documentation for each feature under `./docs/`.
- Update the Feature Index whenever a new feature is introduced.
- Keep feature descriptions short and meaningful.
- Update feature status as development progresses.
- Include a link back to this main README in individual documentation files.

---

**Project:** OpenWrt – LinkIt Smart 7688  
**OpenWrt Version:** v25.12.5  
**Platform:** MediaTek LinkIt Smart 7688  
**Host Environment:** Ubuntu Linux%    

[← Back to Main README](../README.md.md)