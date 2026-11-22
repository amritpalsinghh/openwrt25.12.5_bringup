# Wireless and network — baked into the source

[← Back to Main README](../README.md)

Wi-Fi AP + STA and LAN `192.168.100.1` are set **in the OpenWrt tree**, not by typing `uci` on the board. After flash, first boot applies them. You do **not** run the UCI list by hand.

**Tree:** `openwrt25.12.5_from_scretch/linkit`  
**OpenWrt:** v25.12.5 (`r33051-f5dae5ece4`)  
**Board:** LinkIt Smart 7688 (`ramips` / `mt76x8`)

This uses in-tree **mt76** (`kmod-mt7603` + mac80211). It is **not** 15.05 `mtk-sdk-wifi`. `udhcpc -i apcli0` is not stored as a command; WAN `proto=dhcp` makes netifd run DHCP on the STA.

---

## 1. Expected result after flash (first boot)

| Item | Value |
| --- | --- |
| Ethernet LAN | `192.168.100.1/24` on `eth0` (PC: `192.168.100.10/24`) |
| AP | SSID `XMF-H_37A89B`, open, 2.4 GHz HT20, country TW, channel auto, on `lan` |
| STA | SSID `Zero-Link`, WPA2-PSK, key in image, on `wan` |
| WAN | DHCP client (same role as `udhcpc -i apcli0`) |
| `wireless.default_radio0` | removed if wifi-detect created it |
| Radio | `radio0` enabled (`disabled=0`), path `platform/10300000.wmac` |

On the board (no `uci set` needed):

```bash
uci show wireless
uci show network.lan
uci show network.wan
iwinfo
ifstatus wan
```

**Expect:** AP visible; STA associates to `Zero-Link` if that AP is on air; `wan` gets a DHCP address; LAN is `192.168.100.1`.

Console remains **ttyS2 57600**.

---

## 2. Your UCI vs what is in the source

`channel='1'` then `channel='auto'` → source keeps **`auto`**.

`network.lan.ipaddr='192.168.100.1/24'` → UCI **`ipaddr=192.168.100.1`** + **`netmask=255.255.255.0`** (same prefix).

`udhcpc -i apcli0 -f -q` → **`network.wan.proto=dhcp`**. netifd starts the DHCP client when STA is up.

`ifname=apcli0` is kept as you specified. On 25.12 **mac80211/mt76** the STA name is often `phy0-sta0`, not the 15.05 `apcli0` name. If STA does not come up, remove `option ifname` from the wireless file and rebuild.

SSID `XMF-H_37A89B` is **fixed** in the image (not derived from MAC). STA password is in the squashfs.

---

## 3. Files changed

| Path | Role |
| --- | --- |
| `linkit/files/etc/config/wireless` | Default `/etc/config/wireless` (radio0, `ap`, `sta`). Copied into the image via the OpenWrt `files/` overlay. |
| `linkit/files/etc/uci-defaults/99-xmfh-wifi-network` | Runs **once** on first boot: LAN IP, WAN DHCP, delete `default_radio0`. Then OpenWrt deletes the script. |
| `linkit/.gitignore` | Un-ignore `/files/` so the overlay is tracked (`/files` was ignored by upstream). |
| `linkit/.config` | Minimal Wi-Fi stack **=y**; **ppp** off (not used for this AP+STA). |

Not edited: DTS, `02_network` for other boards, LuCI, Samba.

`linkit/.gitignore` still ignores `/etc/config/wireless` generated on a running board; the **source** copy lives under `files/`.

---

## 4. Minimal `.config` (keep image small)

Already required for this board’s Wi-Fi (leave **=y**):

```
CONFIG_PACKAGE_kmod-mac80211=y
CONFIG_PACKAGE_kmod-cfg80211=y
CONFIG_PACKAGE_kmod-mt76-core=y
CONFIG_PACKAGE_kmod-mt7603=y
CONFIG_PACKAGE_wifi-scripts=y
CONFIG_PACKAGE_wpad-basic-mbedtls=y
CONFIG_PACKAGE_hostapd-common=y
CONFIG_PACKAGE_iw=y
CONFIG_PACKAGE_wireless-regdb=y
```

`wpad-basic-mbedtls` is the small AP+STA daemon (hostapd + wpa_supplicant, mbedtls). Needed because STA is **psk2**. Do **not** enable `wpad-openssl` / `wpad-wolfssl` (larger).

LAN routing (stock router set; keep):

```
CONFIG_PACKAGE_dnsmasq=y
CONFIG_PACKAGE_firewall4=y
CONFIG_PACKAGE_netifd=y
```

**Turned off** (not needed for this network):

```
# CONFIG_PACKAGE_ppp is not set
# CONFIG_PACKAGE_ppp-mod-pppoe is not set
```

**Do not enable** for this step (size): `luci`, `luci-app-wireless`, `samba4`, `python3`, extra `kmod-mt76x2`, `wpad-mesh-*`.

USB/MMC stay on (`DEVICE_PACKAGES` for LinkIt). `helloworld` / `hellocpp` stay as previously selected.

After any `.config` edit:

```bash
cd /home/amrit/XMF_H/openwrt/openwrt25.12.5_from_scretch/linkit
make defconfig
grep -E '^CONFIG_PACKAGE_(kmod-mt7603|wpad-basic-mbedtls|ppp)=' .config
```

---

## 5. How it is applied at boot (you do not run UCI)

```
flash image
  → board.d generates /etc/config/network (lan on eth0)
  → overlay already has /etc/config/wireless
  → wifi-detect skips adding radio0 if path already matches
  → 99-xmfh-wifi-network sets lan 192.168.100.1, wan dhcp, drops default_radio0
  → netifd + wpad bring up AP and STA
  → DHCP on wan (same outcome as udhcpc -i apcli0)
```

`sysupgrade -n` is required if an old overlay already has `/etc/config/wireless` or `/etc/config/network`. Without `-n`, old UCI on the board wins.

---

## 6. Build

```bash
cd /home/amrit/XMF_H/openwrt/openwrt25.12.5_from_scretch/linkit
make -j"$(nproc)"
```

Confirm overlay in the staging root **before** flash:

```bash
grep -n ssid staging_dir/target-mipsel_24kc_musl/root-ramips/etc/config/wireless
grep ipaddr staging_dir/target-mipsel_24kc_musl/root-ramips/etc/config/network || true
ls staging_dir/target-mipsel_24kc_musl/root-ramips/etc/uci-defaults/99-xmfh-wifi-network
```

`network` may still show `192.168.1.1` in squashfs; **99-xmfh-wifi-network** changes it on first boot. After that boot, `uci get network.lan.ipaddr` must be `192.168.100.1`.

---

## 7. Troubleshooting

| Symptom | What to check |
| --- | --- |
| Still `192.168.1.1` | First boot did not run uci-defaults; or overlay UCI from an old flash — use `sysupgrade -n` |
| AP missing | `uci show wireless`; `radio0.disabled` must be `0`; `wifi up` |
| STA never connects | Lab AP `Zero-Link` / key; try removing `option ifname 'apcli0'` |
| `default_radio0` present | Defaults script did not run; `ls /etc/uci-defaults` |
| Image too big | Do not add LuCI/samba/python; keep `wpad-basic-mbedtls`; ppp stays off |

Firmware was not built by this change. Rebuild when you want the new defaults in the sysupgrade.

---

**Feature:** Wireless + LAN/WAN baked into source  
**OpenWrt:** v25.12.5  
**Target:** MediaTek LinkIt Smart 7688
