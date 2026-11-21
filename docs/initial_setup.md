# Initial Setup – OpenWrt v25.12.5

[← Back to Main README](./migration.md)

## 1. Purpose

This guide explains how to clone the parent project repository, run `setup.sh`, and then continue working from the OpenWrt source directory for the **MediaTek LinkIt Smart 7688** target.

The setup script prepares the OpenWrt source and build environment. It does not build the final firmware image and does not create or manage branches, commits, pulls, or pushes in the parent project repository.

## 2. Prerequisites

- Ubuntu Linux build host.
- Git installed.
- Internet connectivity.
- A Git remote URL for the parent project repository.
- `sudo` privileges to install Ubuntu build dependencies (unless dependencies are already installed).
- Sufficient free disk space for the OpenWrt source and subsequent build output.

## 3. Clone the Parent Project Repository

Clone the Repo 
Run the sript 
	cd scripts
	./initial_setup.sh
	or 
	./scripts/initial_setup.sh
openwrt source code will be in linkit folder at root directory 
it is ready to build the source code for Smart Linkit 7688 at minimal confiuration
	

Examples:

```bash

Options:
  --debug, -d       Enable Bash command tracing.
  --skip-deps       Skip Ubuntu package installation.
  --help, -h        Display this help message.

```

## 4. What the Setup Script Does

### 4.1 The script performs the following operations steps:


1. Update Ubuntu package lists | sudo apt-get update

2. Create or verify LinkIt source directory | mkdir -p /home/amrit/XMF_H/openwrt/rinshu/OpenWrt25.12.5_1/linkit
3. Clone OpenWrt v25.12.5 into temporary directory | git clone --depth 1 --branch v25.12.5 https://github.com/openwrt/openwrt.git /tmp/tmp.7stD4iUto2/openwrt
4. Verify OpenWrt source tag | git -C /tmp/tmp.7stD4iUto2/openwrt describe --tags --exact-match
5. Copy OpenWrt source into linkit directory | 
6.  Create feeds.conf from default configuration | cp ./linkit/feeds.conf.default ./linkit/feeds.conf
7. Update OpenWrt feeds. | ./linkit/scripts/feeds update -a
8. Install OpenWrt feeds | ./linikit/scripts/feeds install -aC 
9. Create initial LinkIt Smart 7688 configuration file .config with content
      CONFIG_TARGET_ramips=y
      CONFIG_TARGET_ramips_mt76x8=y
      CONFIG_TARGET_ramips_mt76x8_DEVICE_mediatek_linkit-smart-7688=y
10. Generate OpenWrt defconfig | make -C ./linkit defconfig
11. Check OpenWrt build prerequisites | make -C ./linkit prereq
12. Verify OpenWrt top-level Makefile | test -f ./linkit/Makefile      
13. Verify OpenWrt top-level build include | test -f /./linkit/include/toplevel.mk
14. Verify LinkIt Smart 7688 target configuration |  grep -F CONFIG_TARGET_ramips_mt76x8_DEVICE_mediatek_linkit-smart-7688=y ./linkit/.config

### 4.2 Confirm metadata at last 
```bash
============================================================
       OPENWRT BUILD ENVIRONMENT PREPARATION COMPLETE
============================================================

Parent repository : <parent-repository-path>
OpenWrt source    : <parent-repository-path>/linkit
OpenWrt version   : v25.12.5
Target            : MediaTek LinkIt Smart 7688
Debug mode        : 0

============================================================
```

The script preserves an existing valid OpenWrt source tree and an existing `.config`. If `linkit/` contains partial or unknown files but is not a valid OpenWrt source tree, the script stops rather than overwriting those files.

## 5. After Setup – Continue from the `linkit/` Directory

When the script completes successfully, change into the OpenWrt source directory:

```bash
cd linkit
```

You are now working inside the OpenWrt source tree. Run the following commands from this directory.

### 5.1 Review and customize the target configuration

```bash
make menuconfig
```

Confirm the target is configured for:

- Target System: MediaTek Ralink MIPS (`ramips`)
- Subtarget: MT76x8 (`mt76x8`)
- Target Profile: MediaTek LinkIt Smart 7688

Save the configuration when finished.

### 5.2 Download package sources

```bash
make download -j"$(nproc)"
```

### 5.3 Build the firmware

```bash
make -j"$(nproc)" V=s
```

The first build may take a significant amount of time. `V=s` enables verbose output to help diagnose build errors.

### 5.4 Locate the generated firmware

After a successful build, target images are normally placed under:

```text
bin/targets/
```

For this target, inspect the relevant `ramips/mt76x8` output directory. Exact filenames depend on the build configuration.

## 6. Verify the Setup

You can run these checks from the parent repository root:

```bash
test -f linkit/Makefile && echo "OpenWrt Makefile: OK"
test -f linkit/include/toplevel.mk && echo "OpenWrt build include: OK"
grep -n '^CONFIG_TARGET_ramips_mt76x8_DEVICE_mediatek_linkit-smart-7688=y$' linkit/.config
```

Or, from inside `linkit/`:

```bash
grep -E '^CONFIG_TARGET_(ramips|ramips_mt76x8)' .config
```

## 7. Expected Directory Structure

After successful setup, the repository will contain a structure similar to:

```text
<parent-repo-name>/
├── setup.sh
├── README.md
└── linkit/
    ├── docs/
    │   └── initial_setup.md
    ├── Makefile
    ├── include/
    │   └── toplevel.mk
    ├── scripts/
    │   └── feeds
    ├── feeds.conf
    ├── feeds.conf.default
    ├── .config
    └── ... remaining OpenWrt source files and directories ...
```

## 8. Troubleshooting

### Script reports that it is not in the parent repository root

Ensure that `setup.sh` is located directly in the root of the cloned parent project repository, not in a subdirectory.

### `linkit/` contains files but is not a valid OpenWrt source tree

The script found files in `linkit/` but could not find both `Makefile` and `include/toplevel.mk`. Back up or move the incomplete contents before rerunning the script. Do not remove files unless you have confirmed they are no longer needed.

### Feed update or installation fails

Check internet connectivity and inspect the feed command's error output. Resolve the network or repository issue, then rerun the setup script.

### LinkIt Smart 7688 is not selected

From the parent repository root, run:

```bash
cd linkit
make menuconfig
make defconfig
```

Verify the `ramips` / `mt76x8` target and LinkIt Smart 7688 device profile.

### Build prerequisite check fails

Install the missing Ubuntu package reported by the error, then rerun:

```bash
make prereq
```

## 9. Parent Repository Git Operations

Clone and branch operations are performed manually before running the setup script. The script does not:

- Clone the parent repository.
- Create or switch parent-repository branches.
- Pull or fetch parent-repository changes.
- Commit changes.
- Push to a remote repository.

The script clones the OpenWrt source into a temporary directory and copies it into `linkit/` without the upstream `.git/` directory. Review `.gitignore` and stage only the project files that should be version-controlled.

---

**Author:** Amritpal Singh

[← Back to Main README](./migration.md)
