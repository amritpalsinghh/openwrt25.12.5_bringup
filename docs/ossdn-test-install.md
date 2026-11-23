# Install `ossdn-test` on the device (manual)

[← Back to Main README](../README.md) · [ossdn-test build](./ossdn-test.md)

On this bringup image **`ossdn-test` is not in the firmware**. It is an add-on package (`ossdntest=m`). `command not found` on the board is expected until you copy the APK and install it.

Package name: **`ossdntest`**  
Binary after install: **`/bin/ossdn-test`**  
OpenWrt 25.12.5 uses **`apk`**, not `opkg`.

---

## 1. Find the APK on the PC

```bash
cd /home/amrit/XMF_H/openwrt/openwrt25.12.5_bringup/linkit

find bin/packages -name 'ossdntest*.apk'
# usual path:
ls -l bin/packages/mipsel_24kc/mypackages/ossdntest-*.apk
```

If that file is missing, build it first:

```bash
make package/ossdn-test/compile V=s
```

Expect a name like `ossdntest-1.0.0-r1.apk`. That is the file you transfer. Do **not** copy the host `staging_dir/.../bin/ossdn-test` leftover from an old image build.

Board address (try in this order):

| | Address |
| --- | --- |
| After product LAN defaults | `192.168.100.1` |
| Stock OpenWrt LAN | `192.168.1.1` |
| Console | **ttyS2 57600** (USB–UART on the PC) |

Set `BOARD` to whichever pings:

```bash
BOARD=192.168.100.1
ping -c 2 "$BOARD"
```

---

## 2. Confirm it is missing on the board

SSH (if it works):

```bash
ssh root@$BOARD 'which ossdn-test; ls -l /bin/ossdn-test'
```

Expect: `not found`. After a successful install the same commands show `/bin/ossdn-test`.

---

## 3. Install over SSH

On the **PC**:

```bash
cd /home/amrit/XMF_H/openwrt/openwrt25.12.5_bringup/linkit
scp bin/packages/mipsel_24kc/mypackages/ossdntest-*.apk root@${BOARD}:/tmp/
```

On the **board**:

```bash
ssh root@$BOARD
apk add --allow-untrusted /tmp/ossdntest-*.apk
ossdn-test
ls -l /bin/ossdn-test
ldd /bin/ossdn-test
```

`--allow-untrusted` is required: this APK is local, not from a signed OpenWrt index.

If `apk` complains about a missing `.so`, copy those APKs too (same way) from:

- `bin/packages/mipsel_24kc/mypackages/` — XMF libs (`libi2cinterface`, `libocminterface`, …)
- `bin/packages/mipsel_24kc/packages/` — `libmraa`

With the current image those libs are already inside the squashfs (`=y`), so one `ossdntest` APK is usually enough.

Logout, SSH again, and `ossdn-test` should still run (it is on overlay, not only in `/tmp`).

---

## 4. SSH not working — USB–UART console

Use a USB serial adapter on the PC. Console is **ttyS2, 57600 8N1**. Login `root` (empty password unless you set one).

```bash
# on the PC
screen /dev/ttyUSB0 57600
# or:  picocom -b 57600 /dev/ttyUSB0
```

If Ethernet still answers ping but SSH does not, you can run `apk add` from this console after you get the APK onto `/tmp` (USB stick below, or TFTP).

If there is no network at all, you still need a **file path** onto the board: USB stick (section 5) or a new firmware image via U-Boot USB (section 6).

---

## 5. USB stick (board USB host)

Use this when SSH is down **and** the hardware USB host port works, **and** you can log in on serial (section 4).

**On the PC** — FAT32 stick, APK in the stick root (not a deep folder):

```bash
# example: stick mounted at /media/$USER/USB
cp bin/packages/mipsel_24kc/mypackages/ossdntest-*.apk /media/$USER/USB/
sync
umount /media/$USER/USB
```

Plug the stick into the **board** USB port. On the serial console:

```sh
dmesg | tail
ls /dev/sd*
# typical first partition:
mkdir -p /mnt/usb
mount /dev/sda1 /mnt/usb
ls /mnt/usb/ossdntest*.apk

apk add --allow-untrusted /mnt/usb/ossdntest-*.apk
ossdn-test

umount /mnt/usb
```

If `ls /dev/sd*` is empty, the image likely has **no USB storage** (`kmod-usb-storage` / `kmod-fs-vfat` are unset on this bringup `.config`). Then the stick cannot be mounted. Use section 6, or rebuild the image with those kmods (and/or `ossdntest=y`) and flash that image.

`mount: unknown filesystem type 'vfat'` means the same: `kmod-fs-vfat` is not in the running image.

---

## 6. USB at U-Boot (no Linux SSH)

This does **not** install the APK. It **flashes a firmware image**. Use it when you cannot copy files into a running system.

1. On the PC, copy the **sysupgrade** (not the APK) onto a FAT USB stick:

```text
bin/targets/ramips/mt76x8/openwrt-ramips-mt76x8-mediatek_linkit-smart-7688-squashfs-sysupgrade.bin
```

Rename it on the stick to a short name if U-Boot is picky, e.g. `sysupgrade.bin`.

2. Serial console, interrupt U-Boot. Start USB and list the stick (exact `usb` device number varies; `usb 0` is common):

```
usb start
usb storage
fatls usb 0:1
```

3. Load and flash **only** if you know this board’s U-Boot menu. On the product loader, the numbered USB write that takes `lks7688.img` writes **main** firmware, not golden. Do not write golden unless that is intended.

   After Linux boots from a new image:

   - If that image still has `ossdntest=m`, you still install the APK (sections 3 or 5).
   - If you rebuilt with `ossdntest=y`, `/bin/ossdn-test` is already on the squashfs — no APK step.

Do **not** confuse this with installing `ossdntest-*.apk` from U-Boot. U-Boot cannot `apk add`.

---

## 7. Check after install

```sh
which ossdn-test
ossdn-test
apk info ossdntest
```

`productinfo` still needs `/tmp/product_info.json` — see [ossdn-test.md §7](./ossdn-test.md#7-troubleshooting). That is a later step, not an install failure.

---

## 8. Short map

| Situation | What to do |
| --- | --- |
| `ossdn-test: not found` | Normal — APK not in image. Section 1 then 3 or 5. |
| SSH works | `scp` APK → `apk add --allow-untrusted` |
| SSH dead, serial works, USB stick mounts | Copy APK to FAT stick → mount → `apk add` |
| SSH dead, USB stick not seen | Serial + section 6 (new sysupgrade), or enable USB storage kmods in a new image |
| Want it in every flash | `CONFIG_PACKAGE_ossdntest=y`, rebuild, sysupgrade |

---

**OpenWrt:** v25.12.5 · **Board:** LinkIt Smart 7688 · **APK:** `ossdntest-*.apk`
