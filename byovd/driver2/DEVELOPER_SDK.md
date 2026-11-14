# driver2 — cheat developer SDK (ACE-first, EAC + BE)

Package for a **strong mapped-driver stack**: one map, one IOCTL contract, title profiles for **ACE** (Delta Force, Arena Breakout, ACE-GAME captures), plus **EAC** and **BattlEye**.

**Honest line:** This is a **lab-grade SDK** with measurable gates. It is **not** a promise of undetected live ranked play. Ship your cheat **against `developer_approval.json` on VM**, then integrate on your risk model.

**Cheat inspector (buyer bar):** [`CHEAT_INSPECTOR.md`](CHEAT_INSPECTOR.md) · [`VEHICLE_STEALTH.md`](VEHICLE_STEALTH.md) · `cheat-inspector.ps1` (static **≥90%** without mapping).

**Ban / fingerprint / driver hunting (from dumps):** [`AC_FINGERPRINTS.md`](AC_FINGERPRINTS.md) · `dumps/ac_fingerprints_index.json`.

---

## What you get in the box

| Deliverable | Path |
|-------------|------|
| Kernel payload | `bin/Driver2.sys` |
| Map + profile runner | `bin/Driver2Runner.exe` |
| IOCTL CLI | `bin/Driver2Client.exe` |
| **Integration header** | `sdk/driver2_api.h` |
| **C++ session helper** | `sdk/Driver2Session.hpp` |
| Minimal overlay sample | `examples/cheat_dev_minimal.cpp` |
| **IOCTL catalog** | `IOCTL_REFERENCE.md` |
| **ACE / EAC / BE profiles** | `profiles/*.json` |
| Full stack script | `everything.ps1` |
| Approval gate (≥90%) | `developer-approval-gate.ps1` |
| **Cheat inspector** (S1 static / S2 VM) | `cheat-inspector.ps1` → `cheat_inspector.json` |
| Forensics bundle | `forensics-capture.ps1` |

---

## 5-minute integration (cheat dev flow)

```powershell
# 1) Build once
cd byovd
.\build.ps1
.\driver2\build.ps1

# 2) Map (VM / lab — your call when to -Execute)
.\driver2\Driver2Runner.exe --profile deltaforce --mode bypass --force-map

# 3) In your cheat EXE — link sdk, same as commercial drivers
#    Driver2Session drv; drv.Connect(); drv.ReadProcess(pid, va, buf, n);
```

```cpp
#include "sdk/Driver2Session.hpp"

Driver2Session drv;
if (!drv.Connect()) { /* map first */ }
uint64_t hp = 0;
drv.ReadProcess(gamePid, entityHealthVa, &hp, sizeof(hp));
```

---

## ACE-first (what you expect for Delta / ABI / ACE-GAME)

| Expectation | driver2 answer |
|-------------|----------------|
| ACE callback / DPC hardening | Mapper fires **`0x846`** (strengthen) with profile masks; **`0x86A`** on delta/arena/universal |
| Delta Force dual driver (`ACE-GAME` vs system32) | **IDA:** `dumps/deltaforce-ace_*/kernel/ACE-GAME.sys` — see `profiles/ace-deltaforce.json` |
| Arena Breakout (SGuard + ACE) | Profile `arenabreakout` + `dumps/arenabreakout-ace_*` |
| ACE + EAC + BE in one binary | `--profile universal` + `--driver2` |
| Process R/W for any ACE-protected PID | **`0x805` / `0x806`** after **`0x870`** session |

**ACE map command (dev copy-paste):**

```text
mapper_nal.exe bin\Driver2.sys --driver rtcore --no-auth --driver2 --ac-replica deltaforce --ac-replica-bypass
```

---

## ACE expectations (Delta Force, Arena Breakout) — see `ACE_INTEGRATION.md`

| Expectation | driver2 answer |
|-------------|----------------|
| ACE strengthen | **`0x846`** + `--ace 3f` |
| Delta Force hardening | **`0x84E`** + `--delta 1f` |
| ACE/VGK max | **`0x86A`** + `--ace-vgk` on ACE profiles |
| Runner aliases | `ace`, `delta`, `df`, `abi`, `arena` |
| Post-map API | `Driver2AceStrengthen()` / `Driver2AceDelta()` |
| Captures | `dumps/deltaforce-ace_*`, `dumps/arenabreakout-ace_*` |

## EAC expectations (Rust / EOS) — see also `EAC_INTEGRATION.md`

| Expectation | driver2 answer |
|-------------|----------------|
| EAC improve | **`0x844`** + `--eac 7f` |
| EOS pointer crypto | **`0x851`** + `--encptr f` |
| Runner aliases | `eac`, `eac-rust`, `eos` → `rust` |
| Post-map API | `Driver2EacImprove()` / `Driver2EosEncPtr()` |
| Capture | `dumps/rust-eac-eos_*` |

## BattlEye expectations (R6, ARK SA)

Full guide: **`BE_INTEGRATION.md`**.

| Expectation | driver2 answer |
|-------------|----------------|
| BE improve IOCTL | **`0x852`** with `--ac-replica r6` or `arksa` + `--battleye 1f` |
| Runner aliases | `be`, `be-r6`, `r6s`, `siege` → `r6` |
| Post-map API | `Driver2BattleyeImprove()` / `Driver2Client be-improve` |
| Capture bundle | `dumps/r6-battleye_*` |

```text
mapper_nal.exe bin\Driver2.sys --driver rtcore --no-auth --driver2 --ac-replica r6 --ac-replica-bypass --battleye 1f
```

```text
mapper_nal.exe bin\Driver2.sys --driver rtcore --no-auth --driver2 --ac-replica rust --ac-replica-bypass --eac 7f --encptr f
```

---

## Strong-driver checklist (what pros look for)

### Communication
- [x] Session gate **`0x870`** (binds to your PID)
- [x] Process read/write **`0x805` / `0x806`** (64 KiB chunks on RwTest path; full Betest limits in payload)
- [x] Kernel VA **`0x802` / `0x803`** for debug / chain work
- [x] Device reopen after map (symlink **kept** by default; retries in SDK)
- [x] Dynamic device names (`MsFlt*`, etc.) — not hardcoded `\\.\Cheat`

### Vehicle hide / encrypt / spoof (inspector script)

See **`VEHICLE_STEALTH.md`**. Product stance:

- **Hide** = Tier 1 trace reduction (hygiene, `--inspector` / `--ac-safe`); optional DKOM unlink is **off** by default.
- **Encrypt** = payload lab polish — **not** live RTCore XOR (mapper skips — BSOD).
- **Spoof** = **not** sold — blocklist is load-time hash/cert, not filename cosplay.

### Load / trace (kdmapper-class concerns)
- [x] PiDDB / CI hash / WdFilter hygiene (pre–`DriverEntry`)
- [x] Vehicle IOCTL map (no separate fake `DRIVER_OBJECT`)
- [x] Optional symlink hide (`--driver2-hide-symlink`) — breaks reopen; dev default is **off**
- [ ] Module stomp — **roadmap** (not in tree)
- [ ] IOCTL hijack on resident GPU/filter driver — **Lane D** (not driver2)

### Anti-cheat interaction (lab)
- [x] Preempt callback strip **`0x81B`**
- [x] NMI neut **`0x868`**
- [x] EAC/BE max test **`0x869`**
- [x] ACE/VGK max **`0x86A`** (ACE profiles)
- [x] Title improve IOCTLs **`0x851` / `0x852` / `0x846` / `0x84E`**
- [x] Unified neut **`0x827`** (bypass path)

### Developer experience
- [x] One runner, one client, one header
- [x] Profiles JSON → mapper argv
- [x] `pf_diag.tmp` + `verify-pf-diag.ps1`
- [x] `developer-approval-gate.ps1` (≥90%)
- [x] Forensics snapshot JSON
- [x] Example cheat loop in `examples/`

### What we do **not** sell
- Undetected on live ACE/EAC/BE ranked
- Kernel driver via `sc create` (BYOVD map only)
- Built-in ESP/aimbot — you bring game logic

---

## Profile picker (cheat dev)

| Your game / AC | `--profile` | Notes |
|----------------|-------------|--------|
| Delta Force | `deltaforce` | ACE-GAME.sys for static RE |
| Arena Breakout | `arenabreakout` | ACE + SGuard usermode in dumps |
| Rust | `rust` | EAC EOS |
| R6 / Siege | `r6` | BattlEye |
| ARK SA | `arksa` | BE usermode anchor |
| Multi-title tool | `universal` | All improve masks — heaviest, VM snapshot |
| ACE-only focus | `deltaforce` or `arenabreakout` | Prefer over universal for stability |

JSON: `profiles/ace-deltaforce.json`, `ace-arenabreakout.json`, `eac-rust.json`, `be-r6.json`, `universal.json`.

---

## Verification (what you show another dev)

```powershell
# Static buyer check — no kernel map
.\driver2\cheat-inspector.ps1

# VM full bar (Tier 1 inspector map + approval)
.\driver2\cheat-inspector.ps1 -Execute -ForceMap
```

```powershell
.\driver2\developer-approval-gate.ps1 -Execute -ForceMap -Inspector
```

Outputs `developer_approval.json`. Attach `pf_diag.tmp` + forensics folder for peer review.

**ACE-heavy gate** (Delta + Arena + EAC + BE + universal):

```powershell
.\driver2\developer-approval-gate.ps1 -Execute -ForceMap -IncludeAce
```

---

## Ranked vs lab (split like commercial teams)

| Host | Cheat dev uses |
|------|----------------|
| **Lab VM** | driver2 map + IOCTL + approval gate |
| **Game PC, AC on** | **DMA** reads + offsets; **no map** (Lane D) |
| **Game PC, future** | Hijack comm + your primitive — not driver2 pool device |

---

## File map

```text
driver2/
  DEVELOPER_SDK.md          ← this file
  IOCTL_REFERENCE.md        ← IOCTL table ACE/EAC/BE
  DEVELOPER_APPROVAL.md
  sdk/
    driver2_api.h
    Driver2Session.hpp
  profiles/
  dumps/                    ← EAC + BE + ACE captures (all profiles)
  sync-ac-dumps.ps1
  AC_FINGERPRINTS.md
  build-fingerprint-index.ps1
  examples/
    cheat_dev_minimal.cpp
  bin/
    Driver2.sys
    Driver2Runner.exe
    Driver2Client.exe
```

---

## Support matrix

| AC family | Capture in repo | Profile | Key lab IOCTLs |
|-----------|-----------------|---------|----------------|
| **ACE** | `dumps/deltaforce-ace_*`, `dumps/arenabreakout-ace_*` | `deltaforce`, `arenabreakout` | `0x846`, `0x84E`, `0x86A` |
| **EAC** | `dumps/rust-eac-eos_*` | `rust` | `0x851`, `0x869` |
| **BE** | `dumps/r6-battleye_*` (BEDaisy often missing) | `r6`, `arksa` | `0x852`, `0x869` |

Update captures → refresh hashes in `ac-captures-host/config/ac_host_captures.json`.
