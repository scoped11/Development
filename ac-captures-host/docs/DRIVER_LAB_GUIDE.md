# Driver lab guide — map, read/write, hooks, AC profiles

This bundle holds **anti-cheat captures** from your host plus a **practical guide** to the BYOVD stack in `Development\byovd`. The live sources and binaries are **not duplicated** here; build from the repo paths below.

| Component | Path |
|-----------|------|
| Mapper + build | `Development\byovd\build.ps1` → `byovd\bin\` |
| Full Betest payload | `byovd\src\driver\BetestDriver.c` |
| Minimal R/W payload | `byovd\src\driver\RwTestDrv.c` |
| Usermode IOCTL helpers | `ac-captures-host\sdk\betest_ioctl.h` (copy of `byovd\include\`) |
| Example client | `ac-captures-host\examples\process_rw.cpp` |
| AC → mapper profiles | `ac-captures-host\config\ac_replica_profiles.json` |
| Captured driver hashes | `ac-captures-host\config\ac_host_captures.json` |

---

## 1. What you have in this folder

```
ac-captures-host/
  dumps/          # rust, r6, deltaforce, arenabreakout (+ manifest.json each)
  inbox/          # Kernel .sys for IDA (see MANIFEST.json SHA256)
  config/         # ac_replica_profiles.json, ac_host_captures.json
  sdk/            # betest_ioctl.h
  examples/       # process_rw.cpp
  docs/           # This guide + architecture + IOCTL catalog
  MANIFEST.json
```

**Staging on `D:\driver-lab-ac-dumps`** mirrors `dumps/` for fast copies (OneDrive-safe).

---

## 2. Build (once per change)

```powershell
cd C:\Users\baile\OneDrive\Desktop\Development\byovd
.\build.ps1
```

Outputs:

| File | Role |
|------|------|
| `bin\mapper_nal.exe` | Maps payload via signed vehicle (RTCore, Nal, …) |
| `bin\RwTest.sys` | **Safe lab default** — session auth + process R/W only |
| `bin\Betest.sys` | Full IOCTL surface (AC strips, hooks, DKOM, …) |
| `bin\process_rw.exe` | Example reader (built with mapper) |

Copy a vehicle next to mapper:

```powershell
copy vehicles\RTCore64.sys bin\
# or: vehicles\NalDrvIntel.sys, CorsairLLAccess64.sys, …
```

---

## 3. Map the driver (choose a path)

### Path A — Prove R/W only (low BSOD risk)

Use **RwTest** on a **no-AC snapshot** or with `--ac-safe`:

```powershell
cd byovd
.\scripts\map-ac-replica-lab.ps1 -Profile rust -Mode prove -Driver rtcore
# or manually:
bin\mapper_nal.exe bin\RwTest.sys --driver rtcore --no-auth --ac-safe --ac-replica rust
bin\process_rw.exe
```

Expect: `read 0x1122334455667788`.

Mapper default for Betest is **`--rw-only`**: maps the driver, opens device, **does not** fire strip/kill/DKOM IOCTLs.

### Path B — Title-shaped lab (Betest IOCTLs only)

Per captured game (`config/ac_replica_profiles.json`):

```powershell
.\scripts\map-ac-replica-lab.ps1 -Profile deltaforce -Mode ioctl -Driver rtcore
.\scripts\map-ac-replica-lab.ps1 -Profile r6 -Mode ioctl -Driver rtcore
```

Mapper sets `--ac-replica <name> --ac-replica-ioctl` and runs **title IOCTLs** (EAC `0x851`, BE `0x852`, ACE `0x846`, Delta `0x84E`) — still **avoid** `--full` / `--ac-preempt` on your game PC.

### Path C — Bypass harness (VM snapshot only)

```powershell
.\scripts\map-ac-replica-lab.ps1 -Profile rust -Mode bypass -ForceMap
```

High risk: preempt strip, NMI `0x868`, max bundles `0x869` / `0x86A`. **Revert VM** after each run.

### Profiles ↔ captures

| `--ac-replica` | AC family | Captured dump folder | Primary Betest IOCTLs |
|----------------|-----------|----------------------|------------------------|
| `rust` | EAC EOS | `dumps/rust-eac-eos_*` | `0x851` enc ptr / EAC improve |
| `r6` | BattlEye | `dumps/r6-battleye_*` | `0x852` |
| `deltaforce` | ACE | `dumps/deltaforce-ace_*` | `0x846` + `0x84E` |
| `arenabreakout` | ACE | `dumps/arenabreakout-ace_*` | `0x846` |
| `arksa` | BattlEye | (usermode anchor only) | same as `r6` |

Delta Force static work: use **game-bundle** `ACE-GAME` in dump/inbox (`C3FE27FC…`), not `system32` (`2B6DB055…`).

---

## 4. Open device + session auth

After a successful map, Betest/RwTest exposes a **dynamic** symlink `\\.\MsFlt0` … (prefix list in `betest_ioctl.h`).

```cpp
#include "betest_ioctl.h"

HANDLE h = BetestOpenDevice();  // scans dynamic + static names
if (h == INVALID_HANDLE_VALUE) { /* map failed or wrong name */ }

if (!BetestSessionAuth(h)) { /* all other IOCTLs return ACCESS_DENIED */ }
```

Handshake: `IOCTL_SESSION_AUTH` (`0x870`) — send 8-byte challenge, receive 8-byte token.

RwTest binds session to **authenticating PID**. Betest uses the same gate.

---

## 5. Read / write process memory

Preferred API for overlays and lab tools — avoids usermode `ReadProcessMemory` (Ob callbacks).

**Read** — `IOCTL_READ_PROCESS_MEM` (`0x805`):

```cpp
uint64_t pid = targetPid;
uint64_t addr = gameVa;
uint32_t size = sizeof(value);
uint64_t value = 0;

PROC_READ_REQ req{ pid, addr, size, 0 };
DWORD ret = 0;
DeviceIoControl(h, IOCTL_READ_PROCESS_MEM,
    &req, sizeof(req), &value, size, &ret, nullptr);
```

**Write** — `IOCTL_WRITE_PROCESS_MEM` (`0x806`):

Input buffer = `PROC_WRITE_REQ` header + `Data[size]`. Max chunk **64 KiB** in RwTest (`RwTestDrv.c`).

Helpers: `BetestReadProcess` / `BetestWriteProcess` in `sdk/betest_ioctl.h`.

**Kernel** R/W: `0x802`–`0x804` — easy to BSOD; use only on lab targets.

Full struct layout: `docs/01_read_write.md`.

---

## 6. “Hooking” — three different meanings

### 6a) Usermode client → your mapped driver (normal path)

Your cheat/tool:

1. `CreateFile` → `\\.\MsFltN`
2. `DeviceIoControl` session auth
3. `DeviceIoControl` `0x805` / `0x806` for game VA

No kernel hook required in **your** process.

### 6b) Kernel callback / inline hooks (Betest IOCTLs — opt-in)

Betest can patch **anti-cheat callback registration** when mapper sends IOCTLs (or you send them from your own client):

| IOCTL | Hex | What it does (lab) |
|-------|-----|-------------------|
| Strip all AC callbacks | `0x81B` | `DisableBE/EAC/ACE` — **no-op if AC driver not loaded** |
| Inline hook callbacks | `0x81C` | Upgrade neutralize to inline hooks on AC module ranges |
| NMI neutralize | `0x868` | PG-sensitive |
| EAC improve | `0x844` | Mapper `--eac <hex>` |
| BattlEye improve | `0x852` | Mapper `--battleye <hex>` |
| ACE strengthen | `0x846` | Mapper `--ace <hex>` |
| ACE Delta | `0x84E` | Mapper `--delta <hex>` |

Mapper CLI flags are cataloged in `docs/05_bypass_ioctl_catalog.md`. **Do not stack everything** on the game PC.

Example (VM only):

```text
mapper_nal.exe bin\Betest.sys --driver rtcore --no-auth --ace 3f --delta 1f
```

### 6c) Vehicle IRP wrap (how map works)

`mapper_nal` does **not** register a new `\\.\Betest` service. It:

1. Loads **RTCore64.sys** (or Nal, …) as a service
2. Pool-maps Betest, calls `DriverEntry` with the **vehicle’s** real `DRIVER_OBJECT`
3. Betest `IoCreateDevice` + symlink and sets `IRP_MJ_DEVICE_CONTROL` to its dispatcher
4. Wraps vehicle dispatch so **vehicle IOCTLs** can still reach the original handler for mapping primitives

After map, **do not** `sc stop` the vehicle — use-after-free BSOD. Reboot to unload.

Details: `docs/02_architecture.md`.

### 6d) Live game PC — dispatch hijack on **non-AC** driver (Lane E)

On Rust/R6 with real EAC/BE up, this kit’s **default** is **not** a new BYOVD on the game box — see `docs/22_live_hijack_method.md`:

- Hijack `IRP_MJ_DEVICE_CONTROL` on a **resident non-AC** driver you can open
- Use a **kernel R/W primitive** (0-day class) without loading LOL `.sys`

Betest map on the game PC = high detection + BSOD risk; Lane E is the documented host track.

---

## 7. Mapper flags that affect stability

| Flag | Effect |
|------|--------|
| `--ac-safe` | RwTest path: hygiene, **rw-only**, no AC preempt |
| `--rw-only` (Betest default) | Skip automatic strip/kill/DKOM/mutate after map |
| `--ac-replica <name>` | Title IOCTL masks from `ac_replica_profiles.json` |
| `--ac-replica-ioctl` | Betest + title IOCTL sweep only |
| `--ac-replica-bypass` | Preempt + NMI + max bundles (**VM**) |
| `--ac-preempt` / `--full` | **High BSOD / PatchGuard risk** |
| `--dbg` / `--dkom-*` | **PatchGuard-unsafe** |
| `--no-cleanup` | Keep vehicle loaded after success (required) |

Log: `%TEMP%\pf_diag.tmp` — parse with `byovd\scripts\verify-pf-diag.ps1`.

---

## 8. Build your own client

```powershell
cd ac-captures-host\examples
cl /EHsc /std:c++17 process_rw.cpp /I..\sdk /Feprocess_rw.exe
```

Run only **after** map. For a DLL overlay, link `betest_ioctl.h` helpers the same way.

---

## 9. Workflow cheat sheet

```
[Lab VM snapshot]
  build.ps1
  map RwTest OR Betest (--ac-safe / --ac-replica)
  process_rw.exe  → proof
  your tool → BetestOpenDevice → auth → 0x805/0x806

[Game PC — Lane D/E]
  DMA / split-trust for live read
  OR hijack non-AC driver (driver-lab docs)
  Do NOT rely on this bundle’s bypass on production AC without VM proof
```

---

## 10. Related docs in this bundle

| File | Topic |
|------|--------|
| `01_read_write.md` | IOCTL structs, device names |
| `02_architecture.md` | Map flow, DRIVER_OBJECT, rw-only |
| `07_test_driver.md` | RwTest vs Betest |
| `05_bypass_ioctl_catalog.md` | All mapper flags + IOCTL groups |
| `22_live_hijack_method.md` | Host hijack without new LOL driver |
| `config/ac_replica_profiles.json` | Prove/ioctl/bypass recipes per title |

Canonical copies also live under `Development\byovd\docs\` and `Development\driver-lab\docs\`.
