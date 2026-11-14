# HijackDriver runner (VM lab)

One **Administrator** executable for the **DriverLab** guest: map **RwTest.sys** via **mapper_nal**, shape IOCTL traffic with `--ac-replica <title>`, then prove **session auth + process read** on the mapped device.

This folder is the **usermode lab client**. Kernel payloads and IOCTL implementations live under **`Development\byovd`** (not duplicated here).

| Piece | Location |
|-------|----------|
| This runner | `driver-lab\vm\hijack-driver\HijackDriver.cpp` → `..\bin\HijackDriver.exe` |
| Mapper + payloads | `byovd\bin\mapper_nal.exe`, `RwTest.sys`, `Betest.sys` |
| IOCTL SDK | `byovd\include\betest_ioctl.h` |
| Full map/R/W/bypass guide | `ac-captures-host\docs\DRIVER_LAB_GUIDE.md` |
| AC profile JSON | `ac-captures-host\config\ac_replica_profiles.json` |
| Optional hello driver (service load) | `driver-lab\vm\driver\` → `VmLabDrv.sys` + `VmLabClient.exe` |

**Do not** run mapper or bypass maps on your live ranked PC. Use snapshot VM **`DriverLab-E-26200`** only.

---

## 1. Prerequisites

**Host**

```powershell
cd C:\Users\baile\OneDrive\Desktop\Development\byovd
.\build.ps1
```

**Guest** (after Windows + Guest Additions)

- Shared folder: `D:\hostdev` → your `Development` tree
- Log in as **DriverLab** (admin)
- Vehicle on disk: copy one of `byovd\vehicles\RTCore64.sys` (etc.) into `byovd\bin\` if missing (HijackDriver copies the first available vehicle automatically)

---

## 2. Build HijackDriver

**On host** (output is also visible in guest via share):

```powershell
cd driver-lab\vm\hijack-driver
.\build.ps1
```

Binary: `driver-lab\vm\bin\HijackDriver.exe` (manifest requires Administrator).

Legacy wrapper: `driver-lab\vm\tools\build-hijack-driver.ps1` calls the same build.

---

## 3. Run (map + read proof)

**Guest — elevated PowerShell or cmd:**

```powershell
D:\hostdev\driver-lab\vm\bin\HijackDriver.exe --profile rust --map
D:\hostdev\driver-lab\vm\bin\HijackDriver.exe --profile r6 --map
D:\hostdev\driver-lab\vm\bin\HijackDriver.exe --profile deltaforce --map
```

| Flag | Meaning |
|------|---------|
| `--profile <name>` | Passed to mapper as `--ac-replica` (`rust`, `r6`, `deltaforce`, `arenabreakout`, `arksa`) |
| `--driver rtcore\|nal\|corsair\|…` | Signed vehicle for mapper (default `rtcore`) |
| `--map` | Force map even if device already open |
| `--skip-map` | Device must already be mapped; only auth + read test |
| `BYOVD_ROOT` | Override path to `byovd` (default: `D:\hostdev\byovd` on guest) |

**Success:** `read 0x1122334455667788` and exit code `0`.

**Host triggers guest** (Guest Additions + logged-in admin):

```powershell
.\driver-lab\vm\deploy-run-hijack-driver-guest.ps1 -Profile rust
```

---

## 4. What HijackDriver does internally

1. **`mapper_nal.exe`** loads **`RwTest.sys`** with flags:  
   `--no-auth --ac-safe --ac-replica <profile>`  
   (prove path — not full Betest bypass.)
2. Opens the dynamic device (`BetestOpenDevice()` in `betest_ioctl.h`).
3. **`IOCTL_SESSION_AUTH` (`0x870`)** — session gate; other IOCTLs fail until this succeeds.
4. **`IOCTL_READ_PROCESS_MEM` (`0x805`)** — reads an 8-byte marker from the runner’s own process (same check as `process_rw.exe`).

Mapper command line is printed before launch for audit/replay.

---

## 5. Writing to the driver (process memory)

HijackDriver only **reads** a self-test marker. For **writes** (and arbitrary target PID/VA), use the shared SDK:

**After map + auth** (any client):

```cpp
#include "betest_ioctl.h"

HANDLE h = BetestOpenDevice();
BetestSessionAuth(h);

uint64_t pid = targetPid;
uint64_t addr = gameVa;
uint32_t nbytes = sizeof(value);
uint8_t buf[64];

BetestReadProcess(h, pid, addr, buf, nbytes);

uint32_t value = 0xDEADBEEF;
BetestWriteProcess(h, pid, addr, &value, sizeof(value));
```

| IOCTL | Code | Notes |
|-------|------|--------|
| Session auth | `0x870` | Required first |
| Read process | `0x805` | `PROC_READ_REQ` in/out |
| Write process | `0x806` | `PROC_WRITE_REQ` + data; max **64 KiB** per call on RwTest |

Reference implementation: `ac-captures-host\examples\process_rw.cpp`  
Built artifact: `byovd\bin\process_rw.exe` (after `build.ps1`).

**Kernel** physical/virtual R/W (`0x802`–`0x804`): VM lab only — easy BSOD. See `ac-captures-host\docs\01_read_write.md`.

---

## 6. Beyond prove: Betest + bypass (same VM)

HijackDriver intentionally maps **RwTest** only. For title-shaped **Betest** IOCTL matrices or bypass harness:

```powershell
# Guest Admin
D:\hostdev\driver-lab\vm\setup-vm-lab-betest.ps1 -Mode prove -Profile rust
D:\hostdev\driver-lab\vm\setup-vm-lab-betest.ps1 -Mode ioctl -Profile r6
D:\hostdev\driver-lab\vm\setup-vm-lab-betest.ps1 -Mode bypass -Profile rust   # snapshot first
```

Or on host: `byovd\scripts\map-ac-replica-lab.ps1` (documented in `DRIVER_LAB_GUIDE.md`).

---

## 7. Optional: VmLabDrv (service-loaded practice driver)

Not used by HijackDriver. Teaches **CreateFile + IOCTL** without BYOVD:

```powershell
D:\hostdev\driver-lab\vm\build-vm-lab-driver.ps1    # host or guest with WDK
D:\hostdev\driver-lab\vm\install-vm-lab-driver.ps1  # creates \\.\VmLab0
D:\hostdev\driver-lab\vm\bin\VmLabClient.exe        # auth + read marker
```

See `driver-lab\vm\driver\README.md`.

---

## 8. Troubleshooting

| Symptom | Fix |
|---------|-----|
| `byovd root not found` | Mount `D:\hostdev` or set `BYOVD_ROOT` |
| `Missing mapper or RwTest` | Run `byovd\build.ps1` on host |
| `Driver device not found` | Map failed or reboot cleared manual map; rerun with `--map` |
| `Session auth failed` | Wrong PID/session; remap or run client from same process that authenticated |
| Mapper exit non-zero | Missing vehicle in `byovd\bin\`, blocked driver, or VM snapshot dirty — revert `clean` snapshot |

---

## 9. Related docs

- `driver-lab\docs\26_vm_kernel_driver_lab.md` — VM parity vs live EAC/BE/ACE  
- `driver-lab\docs\25_hijack_host_vm.md` — dispatch hijack **candidates** (separate from this runner)  
- `driver-lab\docs\27_vm_bypass_confidence.md` — L1–L3 gates vs live titles  
- `byovd\docs\04_detection_surfaces.md` — what AC can still see  
- `byovd\docs\05_existing_bypass_catalog.md` — Betest IOCTL / mapper flag appendix  
- `byovd\docs\11_ac_block_vs_hide_plan.md` — Tier 0–3 block vs trace reduction  
- `driver-lab\docs\22_live_hijack_method.md` — **live** architecture (no BYOVD on ranked host)  
- `research\20_SESSION_DRIVER_OPS_AND_STABILITY.md` — ops checklist

---

## 10. Confidence (honest, 2026-11-13)

Nothing here is “undetected on Rust/R6 ranked.” Confidence is **per layer**, not one number for the whole stack.

| Layer | What | Confidence | Why |
|-------|------|------------|-----|
| **HijackDriver.exe** | Builds, invokes mapper, marker read | **~85%** on a **working** DriverLab guest | Thin client; depends on `byovd\bin` + admin + share. **Not validated** end-to-end this session (VM install/GA still in flux). |
| **Prove map** (what HijackDriver runs) | `RwTest.sys` + `--ac-safe --ac-replica` | **~75%** on **no-AC VM** snapshot | Same path as `map-ac-replica-lab.ps1 -Mode prove`. Fails Tier 0 if vehicle blocked while AC up (`exit 2`). |
| **Vehicle hygiene** (Tier 1 “hide”) | PiDDB unlink, CI hash bucket, WdFilter list scrub, `BaseDllName.Length=0`, reg scrub **before** payload `DriverEntry` | **~60%** “works on 26200 lab” | Implemented in `byovd\src\mapper\vehicle_hygiene.hpp`. Pattern/sig drift per KB; vehicle **stays loaded** (not invisible). |
| **Mapper stealth `0x3f`** (with `--ac-safe`) | Timing, thread hide, IOCTL jitter, handle/CR3 helpers (not default DKOM unlink) | **~55%** trace reduction | Reduces **some** self-inflicted signal; does **not** beat hash deny lists or pool scans. |
| **Betest + `--ac-replica-ioctl`** | Title masks → EAC `0x851`, BE `0x852`, ACE `0x846`/`0x84E` | **~50%** on VM if L2 gate passes | Wired in `main_nal.cpp` + `ac_replica_profiles.json`. **Requires** snapshot VM + rebuild; **no** `vm-parity-gate.json` in repo yet → **not auto-proven**. |
| **Betest + `--ac-replica-bypass`** | Preempt strip, NMI `0x868`, max `0x869`/`0x86A` | **~40%** lab / **&lt;10%** live | High BSOD + PG risk; proves **ioctl path fires**, not that Epic/Ubisoft miss you. |
| **Live dispatch hijack (Lane D)** | Patch **existing** non-AC `MajorFunction[14]` + XFG fptr | **~0% shipped** | Documented only (`22_live_hijack_method.md`). No in-tree IRP patch tool; needs **private kW primitive** (Lane E). |
| **Lane E 0-day / CLFS fiction** | `exploit/notes/HYPOTHETICAL_clfs-*` | **0%** (placeholder) | Design doc for plugging primitive into hijack — not a real chain in git. |
| **Production bypass** | Ranked Rust/R6 with BYOVD | **Not claimed** | Host policy + captures say BYOVD vehicles are **expected blocked** with AC up (`ac_eac_be_vehicle_policy.json`). |

**How to raise confidence (objective):**

```powershell
# Guest Admin after clean snapshot + byovd build
D:\hostdev\driver-lab\vm\vm-lab-parity-gate.ps1 -Profile both -ForceMap
D:\hostdev\byovd\scripts\run-timed-harness.ps1 -Profile rust
```

Pass → `driver-lab\exploit\notes\vm-parity-gate.json` with `"all_pass": true` (see `docs/27_vm_bypass_confidence.md`).

---

## 11. What this runner actually uses (active path)

HijackDriver is **Tier 1 prove only**. Exact mapper invocation:

```text
mapper_nal.exe bin\RwTest.sys --driver <vehicle> --no-auth --ac-safe --ac-replica <profile>
```

| Mechanism | On this path? | Detail |
|-----------|---------------|--------|
| Signed BYOVD **vehicle** load | **Yes** | `RTCore64` / `Nal` / `Corsair` copied from `byovd\vehicles\` — stays loaded until reboot |
| **Manual map** of pool payload | **Yes** | Vehicle primitives (`primitive_*.hpp`) — not `sc create` Betest service |
| **`--ac-safe`** | **Yes** | Hygiene on, stealth `0x3f`, **rw-only** (no automatic AC patch IOCTL spray) |
| **`--ac-replica <title>`** | **Yes** | Selects lab profile + logging; **prove branch** keeps `RwTest` (no bypass IOCTL sweep) |
| **`--no-auth`** | **Yes** | Skips mapper usermode auth gate for lab |
| PiDDB / CI / WdFilter hygiene | **Yes** (if hygiene not disabled) | Before hijacking vehicle `MajorFunction` for map |
| PsLoadedModuleList **unlink** for vehicle | **No** (default) | PG `0x109` risk — left off unless explicit DKOM flags |
| Pool **module stomp** | **No** | Not implemented |
| New `\Device\Cheat` for R/W | **No** on RwTest path | RwTest uses **vehicle dispatch wrap** + dynamic `\\.\MsFlt*` for payload IOCTLs |
| Betest callback strip / NMI / `0x869` | **No** in HijackDriver | Use `setup-vm-lab-betest.ps1 -Mode bypass` instead |
| Session PID bind (`0x870`) | **Yes** | Client must auth before `0x805`/`0x806` |
| Process R/W IOCTLs | **Yes** (read in runner; write via SDK) | `0x805` / `0x806` — avoids usermode `ReadProcessMemory` hooks on overlay |

---

## 12. Full stack inventory (everything in the program)

Use this table to see **what exists in-repo** vs **what the live plan is**. “Hide” here means **trace reduction / lab ioctl**, not guaranteed AC bypass.

### 12.1 Lanes

| Lane | Role | Where |
|------|------|--------|
| **A — BYOVD lab** | Map RwTest/Betest on **DriverLab VM** | `byovd/`, this runner, `setup-vm-lab-betest.ps1` |
| **D — Live game reads** | No new LOL `.sys`; future **hijack** comm | `22_live_hijack_method.md` |
| **E — Exploit primitive** | kR/kW without vehicle (fictional/template) | `21_lane_e_exploit_test_track.md`, `exploit/notes/` |
| **C — Static AC** | Captured `.sys` / IDA | `ac-captures-host/`, `driver-lab/dumps/` |

### 12.2 Load / map (BYOVD)

| Technique | Status | Source |
|-----------|--------|--------|
| Vehicle IOCTL primitives (phys/mem map) | **Implemented** | `byovd\src\mapper\primitive_*.hpp` |
| Patch vehicle `DRIVER_OBJECT` for manual map | **Implemented** | `main_nal.cpp` map path |
| Vehicle fallback order (rtcore → corsair → …) | **Implemented** | `main_nal.cpp` |
| `--ac-preflight` / Tier 0 abort | **Implemented** | `11_ac_block_vs_hide_plan.md` |
| `--force-map` | **Implemented** | Lab override only |

### 12.3 Hygiene & “hide” (mapper, pre–DriverEntry)

| Technique | Status | Notes |
|-----------|--------|--------|
| PiDDB AVL unlink | **Implemented** | `vehicle_hygiene.hpp` |
| CI `g_KernelHashBucketList` unlink | **Implemented** | Build-specific patterns |
| WdFilter runtime list scrub | **Implemented** | Pattern-dependent |
| Service registry scrub (no `sc stop` vehicle) | **Implemented** | Vehicle remains loaded |
| `BaseDllName.Length = 0` / name zeroing | **Implemented** | Unload-list skip if ever unloaded |
| PsLoadedModuleList unlink (vehicle) | **Optional / off default** | `--dkom-drv`, PG risk |
| kdmapper-style full mirror | **Not ported** | `05_existing_bypass_catalog.md` “not in kit” rows |

### 12.4 Mapper stealth flags (`0x3f` with `--ac-safe`)

| Bit | Meaning |
|-----|---------|
| `0x01` | Timing / priority boost |
| `0x02` | Hide mapper threads during map |
| `0x04` | IOCTL timing jitter |
| `0x08` | Handle duplication helper |
| `0x10` | CR3 shuffle request post-map |
| `0x20` | DKOM process hide **request** (not default unlink-all) |

### 12.5 Payload comm (after map)

| Technique | RwTest (HijackDriver) | Betest (lab scripts) |
|-----------|----------------------|----------------------|
| Dynamic device symlink pool | **Yes** (`MsFlt*`) | **Yes** |
| `0x870` session auth | **Yes** | **Yes** |
| Process `0x805` / `0x806` | **Yes** | **Yes** |
| Kernel phys/virt `0x802`–`0x804` | Optional | **Yes** |
| `0x808` delete symlink after open | No | **Yes** (Betest) |

### 12.6 Anti-cheat–oriented IOCTLs (Betest only; opt-in)

Invoked via `--ac-replica-ioctl` or `--ac-replica-bypass` — **not** by HijackDriver. Catalog: `05_existing_bypass_catalog.md`. Summary:

| Family | Examples | Mapper/profile hook |
|--------|----------|---------------------|
| EAC improve / strip | `0x807`, `0x80A`, `0x851`, masks in JSON | `--ac-replica rust` |
| BattlEye | `0x852`, BE masks | `--ac-replica r6` |
| ACE / Delta / Arena | `0x846`, `0x84E`, `0x86A` on ACE profiles | `deltaforce`, `arenabreakout` |
| NMI neutralize | `0x868` | bypass / ioctl modes |
| Max lab sweep | `0x869` | `--ac-replica-bypass` |
| Callback strip / unified neut | `0x81B`, `0x827`, preempt path | `--ac-preempt`, bypass |
| DKOM process/thread/driver | `0x82A`, `--dkom-*` | **PG-unsafe**, `--dbg` only |
| Deep hide / mutate / PG resist | `0x82D`, `0x837` | Lab Tier 3 only |

Title-shaped masks: `ac-captures-host\config\ac_replica_profiles.json` ↔ host dumps under `driver-lab\dumps\`.

### 12.7 Live hijack & exploits (planned / research — **not** in HijackDriver)

| Technique | Status | Doc |
|-----------|--------|-----|
| Pick **resident non-AC** driver (GPU/storage/filter) | Research + VM CSV probes | `25_hijack_host_vm.md`, `probe-hijack-candidates.ps1` |
| `MajorFunction[IRP_MJ_DEVICE_CONTROL]` → in-image XFG stub | **Not automated** | `22_live_hijack_method.md` |
| Patch `__guard_dispatch_icall_fptr` → handler | **Not automated** | same |
| Module stomp (handler in signed `.text`) | **Not in tree** | optional layer in method doc |
| Private kR/kW primitive (no `CreateService` vehicle on live) | **You supply** | `lab_manifest.json` `lane_e` |
| CLFS / win32k / pool UAF chains | **Hypothetical note only** | `exploit/notes/HYPOTHETICAL_clfs-*` |
| DMA | **Out of scope** for this runner | user’s separate live read path |

### 12.8 Detection we still expect (even when map “works”)

From `04_detection_surfaces.md` — non-exhaustive:

- Vehicle image still in memory and usually on `PsLoadedModuleList`
- Executable **pool** payload (not a listed PE module) unless stomped
- Patched vehicle dispatch / IOCTL wrap
- AC usermode + kernel integrity, server-side, behavior
- MajorFunction / fptr audits if hijack is attempted

**Engineering default:** stay on **RwTest + prove** until `vm-lab-parity-gate` passes; treat bypass IOCTLs as **forensics on snapshot**, not live ranked defaults.
