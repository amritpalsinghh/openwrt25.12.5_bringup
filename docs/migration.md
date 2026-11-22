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
| helloworld / hellocpp | C and C++ sample apps, image (`=y`) or APK (`=m`) | [helloworld_hellocpp.md](./helloworld_hellocpp.md) | Available |
| Wireless / network | AP+STA and LAN 192.168.100.1 in source | [wireless_network.md](./wireless_network.md) | Available |
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