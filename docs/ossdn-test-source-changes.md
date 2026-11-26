# ossdn-test — source changes vs OpenWrt 15.05

[← Back to Main README](../README.md) · [Build / run](./ossdn-test.md) · [Project phases](./project-phases.md)

This is the record of **every source change** made so `ossdn-test` compiles and links on **OpenWrt v25.12.5**. Product command behaviour was not rewritten.

| | Path |
| --- | --- |
| Legacy (read-only) | `openwrt15.05/mypackages/` |
| Bringup | `openwrt25.12.5_bringup/linkit/mypackages/` |

---

## 0. First fact — `ossdn_test.c` was not changed

`application/ossdn-test/files/src/ossdn_test.c` is **the same** as on 15.05.

The CLI file already compiled as portable C. Failures came from the **libraries it includes and links** (`logMgr`, serial/socket headers, OCM, SFP/I2C, OTDR). Those libraries were copied from 15.05 and then given the smallest GCC 14 / musl edits listed below.

The inner `files/Makefile` and the OpenWrt package `Makefile` **did** change so OpenWrt 25.12.5 can build the binary. That is build plumbing, not CLI logic. See [§7](#7-build-files-not-product-c).

---

## 1. Why 15.05 sources fail on 25.12.5

| | OpenWrt **15.05** (legacy) | OpenWrt **25.12.5** (now) |
| --- | --- | --- |
| Compiler | GCC **4.8** | GCC **14.3** |
| C library | **uClibc** | **musl** |
| Tentative globals in headers | allowed (`-fcommon`) | **error** (`-fno-common` is default) |
| Missing function prototypes | warning / implicit `int` | **error** |
| `pthread_t x = -1` | accepted | **error** (unsigned type) |
| `#include <termio.h>` | present on uClibc | **missing** on musl |

Each change below follows the same pattern:

1. **Problem** — what the compiler/linker said (or would say).
2. **Cause** — why 15.05 allowed it and 25.12.5 does not.
3. **Change** — exact edit.
4. **Why this fix** — same product behaviour; only enough to compile.

Libraries with **no** `.c` / `.h` diff vs 15.05: `HwEventCallback`, `ledinterface`, `batterymanagement`, `crc`, `common`, `powermanagement`, `freescaleinterface`.

---

## 2. Change 1 — `termio.h` → `termios.h`

### Problem

Any translation unit that includes the serial or socket headers fails at **compile**:

```text
fatal error: termio.h: No such file or directory
```

`ossdn-test` pulls those headers through OTDR / OCM / power paths, so the CLI cannot build.

### Cause

15.05 **uClibc** still shipped the old System V header `<termio.h>`.  
**musl** does not. POSIX serial APIs live in `<termios.h>`.

The product code already uses POSIX `termios` types (`struct termios`, `tcgetattr`, …). The include was leftover; it was not using the old System V `struct termio` API.

### Change

Replace the include in four headers:

| File under `linkit/mypackages/libraries/` | 15.05 | 25.12.5 |
| --- | --- | --- |
| `transport/serial/inc/serial.h` | `#include <termio.h>` | `#include <termios.h>` |
| `transport/serial/inc/semaphore_api.h` | `#include <termio.h>` | `#include <termios.h>` |
| `transport/socket/inc/socket_client.h` | `#include <termio.h>` | `#include <termios.h>` |
| `transport/powermgtsocket/inc/power_socket_client.h` | `#include <termio.h>` | `#include <termios.h>` |

Example (`serial.h`):

```c
/* 15.05 */
#include <termio.h>

/* 25.12.5 */
#include <termios.h>
```

### Why this fix

POSIX `termios` is what the code already called. Switching the header does not change baud, framing, or lock-file behaviour.

---

## 3. Change 2 — `pthread_t … = -1` → `= 0`

### Problem

GCC 14 **error**:

```text
error: negative integer implicitly converted to unsigned type [-Werror=sign-conversion]
/* or: invalid conversion / overflow initializing pthread_t with -1 */
```

### Cause

On 15.05, `pthread_t` was treated like a signed `int`, so `-1` meant “no thread yet”.  
On musl / GCC 14, `pthread_t` is an **unsigned** type (often `unsigned long`). Initializing it with `-1` is illegal.

The same files already treat **0** as “thread not created” before `pthread_create` (see `SFPInterfaceInit` in `sfp_interface.c`: `pthread_t readSFPThread = 0`).

### Change

| File | Function | 15.05 | 25.12.5 |
| --- | --- | --- | --- |
| `ocminterface/files/src/ocm_interface.c` | `OCMReset` | `pthread_t OCMtid = -1;` | `pthread_t OCMtid = 0;` |
| `i2cinterface/files/src/sfp_interface.c` | `SFPPower` | `pthread_t SFPtid = -1;` | `pthread_t SFPtid = 0;` |

```c
/* 15.05 */
pthread_t OCMtid = -1;

/* 25.12.5 */
pthread_t OCMtid = 0;
```

### Why this fix

`pthread_create` still overwrites the variable. `0` is the same “unset” sentinel used elsewhere in this tree. Reset / power-switch logic is unchanged.

---

## 4. Change 3 — logger globals: `extern` in the header, define once in `.c`

### Problem

**Link** error (multiple definition), not a compile error in one file:

```text
multiple definition of `_logLevel'
multiple definition of `_logOutput'
```

Every `.c` that included `logMgr.h` contributed another copy of those two symbols. `ossdn-test` links many such objects (`log-maker`, OTDR, OCM, I2C, LED, battery, events).

### Cause

15.05 put **definitions** in the header:

```c
LogLevel _logLevel = LOG_LEVEL_INFO;
int _logOutput = 0;
```

GCC 4.8 / uClibc defaulted to **`-fcommon`**: duplicate tentative definitions were merged at link.  
GCC 14 / musl default **`-fno-common`**: each definition is unique; the linker rejects duplicates.

POSIX / C rule: **declare** in the header, **define** in exactly one `.c`.

### Change

**`libraries/log_maker/files/inc/logMgr.h`**

```c
/* 15.05 */
LogLevel _logLevel = LOG_LEVEL_INFO;
int _logOutput = 0;

/* 25.12.5 */
extern LogLevel _logLevel;
extern int _logOutput;
```

**`libraries/log_maker/files/src/logMgr.c`** — added the single definitions at the top of the file (after the includes):

```c
LogLevel _logLevel = LOG_LEVEL_INFO;
int _logOutput = 0;
```

### Why this fix

Default log level is still `LOG_LEVEL_INFO`; default output mask is still `0`. Callers (`setLogLevel`, `LOG_ERR`, …) still use the same names. Only the storage location is correct C.

---

## 5. Change 4 — `#include "logMgr.h"` in `socket_client.c`

### Problem

GCC 14 **error** on implicit function declaration when this file calls log helpers (`LOG_ERR`, …):

```text
error: implicit declaration of function 'LOG_ERR' [-Wimplicit-function-declaration]
```

(With `-fno-common`, a missing header can also leave `_logLevel` / `_logOutput` undeclared in this unit.)

### Cause

15.05 GCC 4.8 allowed **implicit declarations** (assume `int f()`). GCC 14 treats that as an error.

`socket_client.c` already used the logger. It simply never included `logMgr.h`. Other libraries (`otdr_interface.c`, `ocm_interface.c`, `sfp_interface.c`) already had that include on 15.05.

### Change

**File:** `libraries/transport/socket/src/socket_client.c`

```c
/* 15.05 */
#include "socket_client.h"
#include "otdr_interface.h"

/* 25.12.5 */
#include "socket_client.h"
#include "otdr_interface.h"
#include "logMgr.h"
```

### Why this fix

Adds prototypes (and the `extern` globals from [§4](#4-change-3--logger-globals-extern-in-the-header-define-once-in-c)). Socket protocol, lock file, and Unix-socket path are unchanged.

---

## 6. Change 5 — `SFPInterfaceInit` prototype in `otdr_interface.c`

### Problem

GCC 14 **error**:

```text
error: implicit declaration of function 'SFPInterfaceInit' [-Wimplicit-function-declaration]
```

`otdr_interface.c` **calls** `SFPInterfaceInit()` (OTDR init path). The real function lives in `libi2cinterface` (`sfp_interface.c`).

### Cause

15.05 had the proper header **commented out**:

```c
//#include "sfp_interface.h"
```

Including `sfp_interface.h` pulls **mraa** and forms an include cycle with `libi2cinterface`. GCC 4.8 accepted the call as an implicit `int SFPInterfaceInit()`. GCC 14 does not.

The symbol is still provided at **load time**: package `ossdntest` already depends on both `libotdrinterface` and `libi2cinterface`.

### Change

**File:** `libraries/otdrinterface/files/src/otdr_interface.c`

```c
/* 15.05 */
#include "logMgr.h"
//#include "sfp_interface.h"

/* 25.12.5 */
#include "logMgr.h"
/* sfp_interface.h is not included here: it pulls mraa and would cycle with libi2cinterface.
 * GCC 14 rejects the old implicit declaration; prototype only. Resolved at load time. */
int SFPInterfaceInit(void);
```

### Why this fix

The include stays out (avoids mraa / cycle). Only a prototype is added so GCC 14 can type-check the existing call. OTDR init still calls the same function in `libi2cinterface`.

---

## 7. Change 6 — `select` headers in `sfp_fw_upgrade.c`

### Problem

GCC 14 **error** when the file calls `select()` / uses `struct timeval`:

```text
error: implicit declaration of function 'select'
error: storage size of '...' isn't known   /* struct timeval */
```

This file is part of `libi2cinterface`, which `ossdn-test` links for SFP firmware commands.

### Cause

On 15.05 **uClibc**, `select` and `struct timeval` often arrived as a side effect of other headers (`unistd.h`, `termios.h`, …).  
**musl** is stricter: `select` needs `<sys/select.h>`; `struct timeval` needs `<sys/time.h>`.

The file already used `<termios.h>` for the serial node; it did not include the POSIX select headers.

### Change

**File:** `libraries/i2cinterface/files/src/sfp_fw_upgrade.c`

```c
/* 15.05 */
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/* 25.12.5 */
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>
```

### Why this fix

Makes the existing `select()` wait on the SFP UART compile. Timeout values and lock file `/var/lock/ttyS0.LCK` are unchanged.

---

## 8. Build files (not product C)

These are not `.c` / `.h` behaviour changes. They are required for OpenWrt 25.12.5 to **build** the same sources.

### 8.1 Package Makefile — `application/ossdn-test/Makefile`

| Item | 15.05 | 25.12.5 | Why |
| --- | --- | --- | --- |
| `PKG_BUILD_DIR` override | set | omitted | OpenWrt 25 default is correct; override caused path issues |
| `PKG_LICENSE` | absent | `GPL-2.0-only` | apk metadata |
| Empty `Build/Configure` | omitted (default) | explicit empty | no autoconf |
| `gcc14-legacy.mk` | n/a | included | see §8.3 |
| `CFLAGS` | `$(TARGET_CFLAGS)` only | plus staging includes + `GCC14_LEGACY_CFLAGS` | find json/mraa headers; quiet leftover GCC 14 noise |
| `LDFLAGS` | `$(TARGET_LDFLAGS)` | plus `-L$(STAGING_DIR)/usr/lib` | find the feed libraries |

`PKG_NAME` is still `ossdntest`. Device binary is still `/bin/ossdn-test`. `DEPENDS` is the same library set.

### 8.2 Inner Makefile — `application/ossdn-test/files/Makefile`

OpenWrt’s `Build/Compile` now passes `CFLAGS` and `LDFLAGS`. The inner rules must **use** them.

| Rule | 15.05 | 25.12.5 |
| --- | --- | --- |
| Object compile | `$(CC) -std=gnu99 $(INCLUDES) -c` | `$(CC) -std=gnu99 $(CFLAGS) $(INCLUDES) -c` |
| Link | `$(CC) -std=gnu99 $(INCLUDES) … $(LIB)` | `$(CC) $(LDFLAGS) -std=gnu99 $(INCLUDES) … $(LIB)` |

Without this, `gcc14-legacy.mk` flags and staging include/lib paths never reach `ossdn_test.c`.

### 8.3 `mypackages/gcc14-legacy.mk`

Shared flags for 15.05-era C:

```make
GCC14_LEGACY_CFLAGS:=-Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-pointer-types -Wno-return-mismatch
```

This does **not** replace the source edits. The edits in §§2–7 fix real missing headers, types, and duplicate symbols. The warning flags only keep remaining 15.05-style conversions from stopping the build. Do not treat this file as a substitute for the `.c` / `.h` list.

---

## 9. Summary table

| File | Problem | Change | Reason |
| --- | --- | --- | --- |
| `ossdn-test/.../ossdn_test.c` | — | **none** | already portable C |
| `log_maker/.../logMgr.h` | multiple definition of `_logLevel` / `_logOutput` | `extern` only | `-fno-common` |
| `log_maker/.../logMgr.c` | no single definition | define those two globals | one definition |
| `ocminterface/.../ocm_interface.c` | `pthread_t = -1` | `= 0` | GCC 14 / unsigned `pthread_t` |
| `i2cinterface/.../sfp_interface.c` | `pthread_t = -1` | `= 0` | same |
| `i2cinterface/.../sfp_fw_upgrade.c` | `select` / `timeval` undeclared | `#include <sys/select.h>` and `<sys/time.h>` | musl |
| `otdrinterface/.../otdr_interface.c` | implicit `SFPInterfaceInit` | prototype only; keep `sfp_interface.h` commented | GCC 14; avoid mraa include cycle |
| `transport/serial/inc/serial.h` | missing `termio.h` | `termios.h` | musl |
| `transport/serial/inc/semaphore_api.h` | missing `termio.h` | `termios.h` | musl |
| `transport/socket/inc/socket_client.h` | missing `termio.h` | `termios.h` | musl |
| `transport/socket/src/socket_client.c` | implicit log calls | `#include "logMgr.h"` | GCC 14 prototypes |
| `transport/powermgtsocket/inc/power_socket_client.h` | missing `termio.h` | `termios.h` | musl |
| `ossdn-test/Makefile` | 15.05 build recipe | license, staging flags, `gcc14-legacy.mk` | OpenWrt 25 / apk / GCC 14 |
| `ossdn-test/files/Makefile` | ignored OpenWrt `CFLAGS`/`LDFLAGS` | pass them into compile and link | flags actually apply |

No algorithm, protocol, or `ossdn-test` command-line behaviour was changed.

---

## 10. What this document is not

| Topic | Where |
| --- | --- |
| How to build / `=m` vs `=y` | [ossdn-test.md](./ossdn-test.md) |
| APK install (SSH / USB) | [ossdn-test-install.md](./ossdn-test-install.md) |
| Runtime CLI errors (`productinfo` **1201**, empty `msg:`) | [ossdn-test.md §7](./ossdn-test.md#7-troubleshooting) — missing `/tmp/product_info.json`, not a compile issue |
| 15.05 `libwebsockets` Makefile disabled | [libwebsockets-disabled.md](./libwebsockets-disabled.md) |
| Companion modules for full product boot | [project-phases.md](./project-phases.md) (phase 4) |
| `otdr_emu` / `xmfhwxx` | uClibc prebuilts; **no** musl C sources — not rebuilt |

---

**OpenWrt:** v25.12.5 (GCC 14, musl, `-fno-common`) vs 15.05 (GCC 4.8, uClibc, `-fcommon`)  
**Feed:** `linkit/mypackages`  
**Phase:** 3 (ossdn-test enable) — source record
