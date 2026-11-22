# helloworld and hellocpp — image vs module

[← Back to Main README](../README.md)

Simple C (`helloworld`) and C++ (`hellocpp`) userspace programs for OpenWrt **v25.12.5** on **LinkIt Smart 7688**. This document covers **both** ways to build them:

1. **In the firmware image** (`=y`) — binaries are in the squashfs; they run after flash with no extra install.
2. **As a package/module** (`=m`) — OpenWrt builds an **APK** (not ipk). The image does **not** contain the program; you copy/install the APK on the device.

Do **not** mix `=y` and `=m` for the same package in one `.config`. Pick one mode, run `make defconfig`, then build.

**Work directory:** `openwrt25.12.5_from_scretch/linkit`

---

## 1. What they are

| Package | Language | Source | Install path on device |
| --- | --- | --- | --- |
| `helloworld` | C | `mypackages/helloworld/` | `/usr/bin/helloworld` |
| `hellocpp` | C++ | `mypackages/hellocpp/` | `/usr/bin/hellocpp` |

Feed line (already in `linkit/feeds.conf`; relative so a clone works):

```
src-link mypackages ../../mypackages
```

`hellocpp` depends on **`libstdcpp`**. That is the only extra library. `make defconfig` pulls it in.

---

## 2. One-time feed install

From `linkit/` (after a git clone, also run this — `feeds/` is gitignored):

```bash
cd /home/amrit/XMF_H/openwrt/openwrt25.12.5_from_scretch/linkit
./scripts/feeds update mypackages
./scripts/feeds install helloworld hellocpp
```

Check:

```bash
ls -l feeds/mypackages/helloworld feeds/mypackages/hellocpp
grep 'Package/helloworld' package/feeds/mypackages/helloworld/Makefile
```

---

## 3. Minimal `.config`

Keep the existing board lines. Only add the package lines for the mode you want.

**Required board (already set by initial setup):**

```
CONFIG_TARGET_ramips=y
CONFIG_TARGET_ramips_mt76x8=y
CONFIG_TARGET_ramips_mt76x8_DEVICE_mediatek_linkit-smart-7688=y
```

Then **either** section 4 (image) **or** section 5 (module). After editing `.config`:

```bash
make defconfig
```

`make defconfig` expands dependencies (`libstdcpp` for `hellocpp`). Do not hand-edit hundreds of other symbols.

---

## 4. Build into the firmware image (`=y`)

The programs are copied into the rootfs. After flash they are on the device immediately.

### 4.1 Enable

```bash
cd /home/amrit/XMF_H/openwrt/openwrt25.12.5_from_scretch/linkit
```

Append (or set in `make menuconfig` → Utilities):

```
CONFIG_PACKAGE_helloworld=y
CONFIG_PACKAGE_hellocpp=y
```

```bash
printf '%s\n' \
  'CONFIG_PACKAGE_helloworld=y' \
  'CONFIG_PACKAGE_hellocpp=y' \
  >> .config
make defconfig
grep -E '^CONFIG_PACKAGE_(helloworld|hellocpp|libstdcpp)=' .config
```

**Expect:**

```
CONFIG_PACKAGE_helloworld=y
CONFIG_PACKAGE_hellocpp=y
CONFIG_PACKAGE_libstdcpp=y
```

### 4.2 Build the image

```bash
make -j"$(nproc)"
```

Image:

```text
bin/targets/ramips/mt76x8/openwrt-ramips-mt76x8-mediatek_linkit-smart-7688-squashfs-sysupgrade.bin
```

Confirm they are in the staging root:

```bash
ls staging_dir/target-mipsel_24kc_musl/root-ramips/usr/bin/helloworld
ls staging_dir/target-mipsel_24kc_musl/root-ramips/usr/bin/hellocpp
```

### 4.3 Run on the board

Flash that sysupgrade (`sysupgrade -n`, console **ttyS2 57600**). Then:

```bash
helloworld
hellocpp
which helloworld hellocpp
```

**Expect:** snarky hello strings on stdout; binaries under `/usr/bin/`.

---

## 5. Build as a module / APK only (`=m`)

OpenWrt compiles the package and writes an **APK**. It is **not** installed into the squashfs. Use this to keep the image smaller and install later.

### 5.1 Enable

If you previously set `=y`, change those lines to `=m` (do not leave both).

```bash
cd /home/amrit/XMF_H/openwrt/openwrt25.12.5_from_scretch/linkit
sed -i 's/^CONFIG_PACKAGE_helloworld=y/CONFIG_PACKAGE_helloworld=m/' .config
sed -i 's/^CONFIG_PACKAGE_hellocpp=y/CONFIG_PACKAGE_hellocpp=m/' .config
# if the symbols are missing:
grep -q '^CONFIG_PACKAGE_helloworld=' .config || echo 'CONFIG_PACKAGE_helloworld=m' >> .config
grep -q '^CONFIG_PACKAGE_hellocpp=' .config || echo 'CONFIG_PACKAGE_hellocpp=m' >> .config
make defconfig
grep -E '^CONFIG_PACKAGE_(helloworld|hellocpp|libstdcpp)=' .config
```

**Expect:** `helloworld=m`, `hellocpp=m`. `libstdcpp` may be `=y` (shared runtime) or `=m`; either is fine.

### 5.2 Build only the packages (not a full image)

First world/toolchain must exist (one `make` or at least the toolchain). Then:

```bash
make package/helloworld/compile V=s
make package/hellocpp/compile V=s
```

APKs:

```text
bin/packages/mipsel_24kc/mypackages/helloworld_1.0.0-r1.apk
bin/packages/mipsel_24kc/mypackages/hellocpp_1.0.0-r1.apk
```

Exact filename: `ls bin/packages/mipsel_24kc/mypackages/hello*.apk`

A full `make -j"$(nproc)"` with `=m` also produces those APKs **without** putting `/usr/bin/helloworld` in the image. Check:

```bash
ls staging_dir/target-mipsel_24kc_musl/root-ramips/usr/bin/helloworld 2>/dev/null || echo "not in image (expected for =m)"
```

### 5.3 Install on the board

Copy APKs (and `libstdcpp` if it is not already on the image):

```bash
scp bin/packages/mipsel_24kc/mypackages/helloworld_*.apk \
    bin/packages/mipsel_24kc/mypackages/hellocpp_*.apk \
    root@192.168.1.1:/tmp/
```

On the device (OpenWrt 25.12.5 uses **apk**):

```bash
apk add --allow-untrusted /tmp/helloworld_*.apk /tmp/hellocpp_*.apk
helloworld
hellocpp
```

If `hellocpp` fails with a missing `libstdc++.so`, also copy and add the `libstdcpp` APK from `bin/packages/mipsel_24kc/base/` (or set `CONFIG_PACKAGE_libstdcpp=y` so the image always has it).

---

## 6. `=y` vs `=m`

| | Image (`=y`) | Module (`=m`) |
| --- | --- | --- |
| In squashfs | Yes | No |
| Output | sysupgrade `.bin` | `.apk` under `bin/packages/.../mypackages/` |
| On device after flash | `helloworld` already in PATH | must `apk add` |
| Image size | slightly larger | smaller |
| Typical command | `make -j$(nproc)` | `make package/<name>/compile V=s` |

---

## 7. menuconfig (optional)

```bash
make menuconfig
```

Utilities → `helloworld` / `hellocpp`:

- `*` → `=y` (image)
- `M` → `=m` (APK only)
- empty → not built

Save, then `make defconfig`.

---

## 8. Troubleshooting

| Symptom | Fix |
| --- | --- |
| Package not in menuconfig | `./scripts/feeds update mypackages && ./scripts/feeds install helloworld hellocpp` |
| `src-link` fails | From `linkit/`, `ls ../../mypackages/helloworld/Makefile` must exist |
| Recipe “missing separator” | Package Makefiles need **tabs**, not spaces |
| `hellocpp` link errors | `DEPENDS:=+libstdcpp` is already in the Makefile; run `make defconfig` |
| `apk` not found on device | You are on 15.05 (opkg). This tree is 25.12.5 apk |
| Binary is x86 | Inner Makefile must use `$(CC)` / `$(CXX)`, not `gcc` |

No firmware was built by writing this document. Enable `=y` or `=m` in `.config`, then build when you are ready.

  Find the modules 
     find bin/ -type f -name "*.apk" | grep hello
     or 
     find bin/ -type f \( -iname '*helloworld*.apk' -o -iname '*hellocpp*.apk' \)

  say output 
      bin/packages/mipsel_24kc/mypackages/hellocpp-1.0.0-r1.apk
      bin/packages/mipsel_24kc/mypackages/helloworld-1.0.0-r1.apk
---

**Feature:** helloworld / hellocpp  
**OpenWrt:** v25.12.5  
**Target:** MediaTek LinkIt Smart 7688
