# ossdn-test — `.c` / `.h` changes vs 15.05

[← Back to Main README](../README.md) · [ossdn-test build/run](./ossdn-test.md)

This lists **every `.c` and `.h` difference** between legacy **OpenWrt 15.05** (`openwrt15.05/mypackages`) and the bringup copy used to compile **`ossdn-test`** on **OpenWrt 25.12.5** (`openwrt25.12.5_bringup/linkit/mypackages`).

Behaviour of the CLI is unchanged. These edits exist so GCC 14 + **musl** will compile and link. Product logic was not rewritten.

**Compared trees**

| | Path |
| --- | --- |
| Legacy | `openwrt15.05/mypackages/` |
| Bringup | `openwrt25.12.5_bringup/linkit/mypackages/` |

15.05: GCC **4.8**, **uClibc**, `-fcommon` (tentative definitions in headers were allowed).  
25.12.5: GCC **14.3**, **musl**, default **`-fno-common`**.

---

## 1. `ossdn_test.c` — not changed

`application/ossdn-test/files/src/ossdn_test.c` is **byte-identical** to 15.05.

The CLI source did not need edits. Compile/link failures came from the **libraries** it includes (`logMgr.h`, OTDR/OCM/SFP, serial/socket headers).

(The inner `files/Makefile` differs for OpenWrt flags; that is not a `.c`/`.h` change.)

---

## 2. Why 15.05 sources fail on 25.12.5

| Legacy (15.05) | 25.12.5 | What breaks |
| --- | --- | --- |
| `#include <termio.h>` | musl has **no** `termio.h` | missing header |
| `pthread_t x = -1` | `pthread_t` is an unsigned type | GCC 14 **error**: negative value |
| `LogLevel _logLevel = ...` in a **header** | `-fno-common` | multiple-definition at link |
| Implicit function declarations | GCC 14 errors | `SFPInterfaceInit`, `select`, log helpers |
| glibc/uClibc pulled `select` via other headers | musl is stricter | `select` undeclared in `sfp_fw_upgrade.c` |

Libraries with **no** `.c`/`.h` diff vs 15.05: `HwEventCallback`, `ledinterface`, `batterymanagement`, `crc`, `common`, `powermanagement`, `freescaleinterface`.

---

## 3. Change list (legacy → bringup)

### 3.1 `termio.h` → `termios.h` (musl)

**Reason:** musl does not ship System V `termio.h`. POSIX serial APIs live in `termios.h`. 15.05 uClibc still had `termio.h`. The code only needs the POSIX header (it already uses `termios` types).

| File (under `linkit/mypackages/libraries/`) | Legacy | Now |
| --- | --- | --- |
| `transport/serial/inc/serial.h` | `#include <termio.h>` | `#include <termios.h>` |
| `transport/serial/inc/semaphore_api.h` | `#include <termio.h>` | `#include <termios.h>` |
| `transport/socket/inc/socket_client.h` | `#include <termio.h>` | `#include <termios.h>` |
| `transport/powermgtsocket/inc/power_socket_client.h` | `#include <termio.h>` | `#include <termios.h>` |

Without this, any TU that includes these headers (OTDR/OCM/power paths used by `ossdn-test`) fails at compile.

---

### 3.2 `pthread_t … = -1` → `= 0` (GCC 14)

**Reason:** `pthread_t` is not a signed `int`. GCC 14 rejects initializing it with `-1`. `0` is the invalid/unset id this code already treats as “no thread” before `pthread_create`.

| File | Legacy | Now |
| --- | --- | --- |
| `ocminterface/files/src/ocm_interface.c` (`OCMReset`) | `pthread_t OCMtid = -1;` | `pthread_t OCMtid = 0;` |
| `i2cinterface/files/src/sfp_interface.c` (`SFPPower`) | `pthread_t SFPtid = -1;` | `pthread_t SFPtid = 0;` |

---

### 3.3 Logger globals: header `extern`, define once in `.c` (`-fno-common`)

**Reason:** 15.05 put **definitions** in `logMgr.h`. Every `.c` that included the header got its own `_logLevel` / `_logOutput`. uClibc/GCC 4.8 merged those (`-fcommon`). GCC 14 / musl default **`-fno-common`**: link error (multiple definition). POSIX rule: declare in the header, define in **one** `.c`.

**`libraries/log_maker/files/inc/logMgr.h`**

| Legacy | Now |
| --- | --- |
| `LogLevel _logLevel = LOG_LEVEL_INFO;` | `extern LogLevel _logLevel;` |
| `int _logOutput = 0;` | `extern int _logOutput;` |

**`libraries/log_maker/files/src/logMgr.c`** — added the single definitions:

```c
LogLevel _logLevel = LOG_LEVEL_INFO;
int _logOutput = 0;
```

---

### 3.4 `#include "logMgr.h"` in `socket_client.c`

**File:** `libraries/transport/socket/src/socket_client.c`

**Reason:** this file calls log helpers. 15.05 compiled with implicit declarations. GCC 14 errors on that. Including `logMgr.h` gives the prototypes (and uses the `extern` globals from §3.3).

Legacy: no `logMgr.h`.  
Now: `#include "logMgr.h"` after `otdr_interface.h`.

---

### 3.5 `SFPInterfaceInit` prototype in `otdr_interface.c`

**File:** `libraries/otdrinterface/files/src/otdr_interface.c`

**Reason:** 15.05 had `sfp_interface.h` commented out (including it pulls **mraa** and cycles with `libi2cinterface`). The `.c` still **calls** `SFPInterfaceInit()`. GCC 4.8 allowed an implicit `int` declaration. GCC 14 does not.

The symbol is still provided at **load time** by `libi2cinterface` (`ossdntest` already depends on both libraries). Only a prototype is added; the include stays out.

| Legacy | Now |
| --- | --- |
| `//#include "sfp_interface.h"` | comment + `int SFPInterfaceInit(void);` |

---

### 3.6 `select` headers in `sfp_fw_upgrade.c`

**File:** `libraries/i2cinterface/files/src/sfp_fw_upgrade.c`

**Reason:** the file calls `select()`. On 15.05 uClibc, `select` often arrived through other includes. musl requires `<sys/select.h>` (and `<sys/time.h>` for `struct timeval`). Without them GCC 14 reports an implicit declaration / unknown type.

Added after the existing includes (the file already used `<termios.h>`):

```c
#include <sys/select.h>
#include <sys/time.h>
```

---

## 4. Summary table

| File | Legacy | Change | Why |
| --- | --- | --- | --- |
| `ossdn-test/.../ossdn_test.c` | product CLI | **none** | already portable C |
| `log_maker/.../logMgr.h` | defined `_logLevel`, `_logOutput` | `extern` only | `-fno-common` |
| `log_maker/.../logMgr.c` | no globals | define those two | one definition |
| `ocminterface/.../ocm_interface.c` | `pthread_t = -1` | `= 0` | GCC 14 / unsigned `pthread_t` |
| `i2cinterface/.../sfp_interface.c` | `pthread_t = -1` | `= 0` | same |
| `i2cinterface/.../sfp_fw_upgrade.c` | no select headers | `sys/select.h`, `sys/time.h` | musl |
| `otdrinterface/.../otdr_interface.c` | implicit `SFPInterfaceInit` | prototype only | GCC 14; avoid mraa include cycle |
| `transport/serial/inc/serial.h` | `termio.h` | `termios.h` | musl |
| `transport/serial/inc/semaphore_api.h` | `termio.h` | `termios.h` | musl |
| `transport/socket/inc/socket_client.h` | `termio.h` | `termios.h` | musl |
| `transport/socket/src/socket_client.c` | no `logMgr.h` | include it | GCC 14 prototypes |
| `transport/powermgtsocket/inc/power_socket_client.h` | `termio.h` | `termios.h` | musl |

No algorithm, protocol, or `ossdn-test` command-line behaviour was changed.

---

## 5. What this is not

- OpenWrt **package Makefiles** (`gcc14-legacy.mk`, `Build/InstallDev`) — see [ossdn-test.md](./ossdn-test.md).
- Runtime CLI errors (e.g. `productinfo error: 1201`) — see [ossdn-test.md §7](./ossdn-test.md#7-troubleshooting).
- `additional_files/` (unused for this CLI).
- `otdr_emu` / `xmfhwxx` (uClibc binaries; no musl C sources).

---

**OpenWrt:** v25.12.5 (GCC 14, musl) vs 15.05 (GCC 4.8, uClibc)  
**Feed:** `linkit/mypackages`
