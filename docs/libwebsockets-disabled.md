# Why `libwebsockets` is disabled in `additional_files`

[← Back to Main README](../README.md) · [ossdn-test](./ossdn-test.md)

This note is only about:

```text
linkit/mypackages/additional_files/libwebsockets/Makefile.15.05.disabled
```

It is **not** part of `ossdn-test`. Leave it disabled.

---

## 1. What you are looking at

| | Path |
| --- | --- |
| Bringup (disabled) | `openwrt25.12.5_bringup/linkit/mypackages/additional_files/libwebsockets/Makefile.15.05.disabled` |
| Legacy (active Makefile) | `openwrt15.05/mypackages/additional_files/libwebsockets/Makefile` |
| 25.12.5 replacement | `linkit/feeds/packages/libs/libwebsockets/Makefile` (packages feed) |

The bringup file is a **rename** of the 15.05 Makefile. Contents are the same as legacy. OpenWrt’s feed scanner only indexes a directory that contains a file named **`Makefile`**. The `.disabled` suffix means this package is **not** in `feeds/mypackages.index`, **not** in menuconfig, and **not** built.

The sources were kept so you can read what 15.05 had. They are not selected.

---

## 2. What legacy (15.05) had

On 15.05 this was a **vendored copy** of the OpenWrt **packages** `libwebsockets` recipe, dropped into the custom feed `mypackages/additional_files/`.

It is **byte-identical** to `openwrt15.05/feeds/packages/libs/libwebsockets/Makefile` (same version, same variants, same cmake flags). It is **not** an XMF-H library and **not** used by `ossdn-test`.

**Package facts (15.05 mypackages copy):**

| Item | Legacy value |
| --- | --- |
| `PKG_NAME` | `libwebsockets` |
| Upstream | [warmcat/libwebsockets](https://github.com/warmcat/libwebsockets) **v3.2.3** |
| Build | CMake (`include cmake.mk`) |
| Variants | `libwebsockets-openssl`, `libwebsockets-mbedtls`, `libwebsockets-full` |
| `PROVIDES` | `libwebsockets` (virtual name) |
| `DEPENDS` | `+zlib +libcap +libpthread +libevent2 +libevent2-pthreads` (+ SSL per variant) |
| Patch | `patches/020-fix-travis.patch` — strips `-Werror` from upstream `CMakeLists.txt` so GCC 4.8/warnings do not fail the build |

CMake options on that old recipe (not the 25.12.5 packages recipe): IPv6 off, test apps **on**, HTTP2 off, libuv/libev off, `LWS_HAVE_LIBCAP=ON`, server enabled. `full` variant adds OpenSSL, CGI, access log, unix sockets.

**Who used it on 15.05**

- `ossdn-test` **does not** depend on it (`DEPENDS` is mraa + XMF interface libs only).
- `mtk-linkit-webui` is static HTML/JS under `/www`; its Makefile has **no** `libwebsockets` depend.
- `mtk-linkit` (board meta package) also has **no** `libwebsockets` depend.

So on the product this was a **generic websocket library** sitting in the custom feed, duplicating the official packages feed even on 15.05. Nothing in the XMF-H CLI path requires it.

---

## 3. Why it is disabled on 25.12.5

**Same package name, two feeds.** If this directory still had a `Makefile`, `./scripts/feeds update mypackages` would register `PKG_NAME:=libwebsockets` from **mypackages**. The **packages** feed already has that name:

| | 15.05 mypackages (disabled copy) | 25.12.5 packages feed (keep this) |
| --- | --- | --- |
| Version | **3.2.3** | **4.4.1** |
| License in Makefile | LGPL-2.1+exception | MIT |
| Extra deps | `libevent2`, `libevent2-pthreads` | `libubox`; `full` also `libuv` |
| Test apps | built (`LWS_WITHOUT_TESTAPPS=OFF`) | off |
| `CONFLICTS` between SSL variants | no | yes (`openssl` vs `full` / `mbedtls`) |

Two recipes for `libwebsockets` / `libwebsockets-openssl` / `libwebsockets-mbedtls` / `libwebsockets-full` would:

1. Collide in the package index (`feeds install` / menuconfig: which feed wins is undefined / last-wins).
2. Build the **wrong** ABI (3.2 vs 4.4 headers and `.so`).
3. Pull extra 15.05-era deps (`libevent2`) that the current packages recipe does not use the same way.
4. Fail or warn under GCC 14 / musl even with the old `-Werror` patch (that patch only removes `-Werror`; it does not make 3.2.3 a 25.12.5-first-class package).

**ossdn-test does not need it.** Enabling it would not fix `productinfo` 1201 or any I2C/CLI command.

Rename (not delete) was the minimum: keep the 15.05 text for reference, stop the feed scanner.

---

## 4. How the rename works

OpenWrt `scripts/feeds` walks `mypackages/` and only treats a folder as a package if it contains **`Makefile`**.

| Filename | Indexed? |
| --- | --- |
| `Makefile` | yes — would clash |
| `Makefile.15.05.disabled` | **no** |

Check:

```bash
cd /home/amrit/XMF_H/openwrt/openwrt25.12.5_bringup/linkit
grep libwebsockets feeds/mypackages.index && echo 'still indexed (bad)' || echo 'not in mypackages index (good)'
grep -A2 'Package: libwebsockets-openssl' feeds/packages.index | head
```

Expect: **no** hit in `mypackages.index`; **yes** in `packages.index` (4.4.1).

---

## 5. If you actually need libwebsockets later

Do **not** rename the disabled file back to `Makefile`.

Use the packages feed:

```bash
cd /home/amrit/XMF_H/openwrt/openwrt25.12.5_bringup/linkit
./scripts/feeds update packages
./scripts/feeds install libwebsockets-openssl   # or libwebsockets-mbedtls / libwebsockets-full
```

Then in `.config` pick **one** variant (`=y` or `=m`). Do not mix the three.

That is the supported 25.12.5 library. The 15.05 3.2.3 copy stays a museum file.

---

## 6. What this is not

- Not an `ossdn-test` compile fix.
- Not a replacement for XMF interface libraries under `mypackages/libraries/`.
- Not required for helloworld / hellocpp.
- Other files under `additional_files/` (webui, `51_linkit_config`, banner, …) are a separate topic; they are also **not** selected for the current ossdn-test bringup.

---

**OpenWrt:** v25.12.5 packages `libwebsockets` 4.4.1 vs 15.05 mypackages copy 3.2.3  
**Action:** leave `Makefile.15.05.disabled` as-is
