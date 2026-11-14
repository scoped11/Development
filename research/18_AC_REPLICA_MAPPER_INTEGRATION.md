# AC replica profiles ↔ BYOVD mapper (session integration)

**Edition:** 13 November 2026 | Host `26200.9974` | Implements capture metadata into lab tooling.

Companion: [17_HOST_AC_CAPTURES_2026-11-13.md](17_HOST_AC_CAPTURES_2026-11-13.md), `byovd/docs/ac_replica_profiles.json` (schema **v2**), `byovd/docs/ac_host_captures.json`.

## 1. Problem

Lab VM must exercise **title-shaped** Betest IOCTL paths (EAC `0x851`, BE `0x852`, ACE `0x846`, Delta `0x84E`) using **hashes and builds from the real host**, not generic CTF defaults.

## 2. Mapper: `--ac-replica` expansion

**File:** `byovd/src/mapper/main_nal.cpp`

| Value | Alias | Family | Title IOCTL masks (ioctl / bypass) |
|-------|-------|--------|-----------------------------------|
| `rust` | — | EAC EOS | `g_eacImpFlags 0x7F` |
| `r6` | — | BattlEye | `g_battleyeFlags 0x1F` |
| `deltaforce` | `delta` | ACE | `g_aceStrFlags 0x3F`, `g_deltaFlags 0x1F` |
| `arenabreakout` | `abi` | ACE | `g_aceStrFlags 0x3F` |
| `arksa` | `ark` | BattlEye | same as `r6` |

**Bypass-only (VM):** `g_aceVgkMax` (IOCTL `0x86A`) for ACE profiles 3–4; `g_eacBeMax` (`0x869`) all bypass profiles.

Helpers: `AcReplicaProfileName()`, `ApplyAcReplicaTitleMasks()` inside `ApplyAcReplicaLabProfile()`.

## 3. Scripts updated

| Script | Change |
|--------|--------|
| `byovd/scripts/map-ac-replica-lab.ps1` | `ValidateSet` includes deltaforce, arenabreakout, arksa |
| `byovd/scripts/verify-pf-diag.ps1` | `ace846`, `ace84e`, `ace86a`; profile gates for ACE titles |
| `byovd/scripts/run-timed-harness.ps1` | Profile set + SGuard process patterns |
| `driver-lab/vm/vm-lab-parity-gate.ps1` | `Profile ace` (DF+ABI), `all`; AC process match extended |

## 4. Replica recipes (from JSON)

Each profile defines `prove` / `ioctl` / `bypass` mapper flag arrays and `pass_criteria` tied to `pf_diag.tmp`.

Example — Delta Force bypass stack:

- `--ac-replica deltaforce --ac-replica-bypass`
- Expect: ACE strengthen + Delta harden in log; pf_diag `0x846` + `0x84E` (ioctl); bypass adds strip + `0x869` + `0x86A` when aggressive path enabled

**Live host split:** Production Rust/R6/ACE titles → Lane D (DMA/split-trust) + Lane E (hijack), not ranked bypass claims on game PC. See [20_SESSION_DRIVER_OPS_AND_STABILITY.md](20_SESSION_DRIVER_OPS_AND_STABILITY.md).

## 5. Vehicle policy

`driver-lab/data/ac_eac_be_vehicle_policy.json` — added **`paid_ace`** tier (RTCore/Nal block patterns, `\\.\ACE-GAME` device hint) with sources pointing at Delta/ABI dumps.

## 6. Verification commands (VM snapshot)

```powershell
cd Development\byovd
.\build.ps1
.\scripts\map-ac-replica-lab.ps1 -Profile deltaforce -Mode ioctl -Driver rtcore
.\scripts\verify-pf-diag.ps1 -Profile deltaforce

cd ..\driver-lab\vm
.\vm-lab-parity-gate.ps1 -Profile ace -ForceMap   # VM only
```

## 7. Build verification

`build.ps1` succeeded after profile integration (mapper + Betest + RwTest + process_rw).
