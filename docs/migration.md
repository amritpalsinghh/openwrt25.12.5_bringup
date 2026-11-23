[← Back to Main README](../README.md)
# OpenWrt – LinkIt Smart 7688
 
OpenWrt v25.12.5 migration, customization and development for the MediaTek LinkIt Smart 7688 platform.

**Note** This docuement is yet not final. 

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Migration Workflow](#3-migration-workflow)
3. [Feature Index](#4-feature-index)
4. [Status Definitions](#5-status-definitions)
5. [Documentation Guidelines](#6-documentation-guidelines)

---

## 1. Project Overview

This project focuses on migrating and customizing OpenWrt for the MediaTek LinkIt Smart 7688 platform.

It covers environment setup, system configuration, package development, networking, firmware generation, deployment and hardware validation.


## 2. Migration Workflow

The overall development and migration process follows these phases:

1. **Initial Setup** – Setup initial makeable/buildable setup. [← Refer to Intial Setup Script](./initial_setup.md)
2. Make/build it and update the .gitignore file
3. Network Stack update
4. So on ... (update soon... )


## 3. Feature Index

Individual feature documentation, implementation details and usage instructions are maintained under `linkit/docs/`.

| Feature | Description | Documentation | Status |
|---|---|---|---|
| Initial Setup | Development environment and source preparation | [initial_setup.md](./initial_setup.md) | Available |
| helloworld / hellocpp | C/C++ samples at `linkit/mypackages/application/` (`=m` APK) | [helloworld_hellocpp.md](./helloworld_hellocpp.md) | Available |
| ossdn-test | 15.05 CLI at `linkit/mypackages/application/ossdn-test` | [ossdn-test.md](./ossdn-test.md) | Available |
| ossdn-test `.c`/`.h` | musl/GCC14 source diffs vs 15.05 | [ossdn-test-source-changes.md](./ossdn-test-source-changes.md) | Available |
| ossdn-test runtime | `productinfo` 1201 / empty msg | [ossdn-test.md §7](./ossdn-test.md#7-troubleshooting) | Available |
| ossdn-test install | APK search, SSH, USB stick, U-Boot USB | [ossdn-test-install.md](./ossdn-test-install.md) | Available |
| libwebsockets (15.05 copy) | why `Makefile.15.05.disabled`; use packages 4.4.1 | [libwebsockets-disabled.md](./libwebsockets-disabled.md) | Available |
| GitHub tags | `v0.0.1`–`v0.0.4` + `v0.0.5` ossdn-test docs | [github-tags.md](./github-tags.md) | Available |
| Wireless / network | left as reset | [wireless_network.md](./wireless_network.md) | Left as-is |
| Configuration | OpenWrt target and system configuration | [target_configuration.md](./linkit/docs/target_configuration.md) | Planned |
| Package Development | Custom package development and integration | [package_development.md](./linkit/docs/package_development.md) | Planned |
| Wi-Fi Configuration | Wireless interface configuration | [wifi_configuration.md](./linkit/docs/wifi_configuration.md) | Planned |
| Network Configuration | Interfaces, DHCP and SSH configuration | [network_configuration.md](./linkit/docs/network_configuration.md) | Planned |
| Firmware Build | OpenWrt firmware compilation | [firmware_build.md](./linkit/docs/firmware_build.md) | Planned |
| Firmware Deployment | Firmware flashing and deployment | [firmware_deployment.md](./linkit/docs/firmware_deployment.md) | Planned |
| Testing & Validation | Hardware and functional validation | [testing.md](./linkit/docs/testing.md) | Planned |

## 4. Status Definitions

| Status | Description |
|---|---|
| Available | Implementation and documentation are available. |
| In Progress | Feature development is ongoing. |
| Planned | Feature is identified for future development. |
| Completed | Implementation and validation are complete. |

## 5. Documentation Guidelines

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