# driver2 — BattlEye (BE) integration

First-class BE path for **R6** (`r6`) and **ARK SA** (`arksa`) with the same driver2 map + IOCTL contract as EAC/ACE.

## Quick map (VM lab)

```powershell
.\driver2\build.ps1
.\bin\Driver2Runner.exe --profile r6 --mode bypass --force-map --inspector
# or
.\driver2\everything.ps1 -Profile r6 -Target vm -Execute -ForceMap
```

Aliases: `--profile be`, `be-r6`, `r6s`, `siege`, `battleye` → `r6`; `be-ark`, `ark` → `arksa`.

## What runs on BE profiles

| Stage | Behavior |
|-------|----------|
| Mapper | `--ac-replica r6` or `arksa` + `--driver2` + `--ac-replica-bypass` |
| Masks | `g_battleyeFlags = 0x1F` (CB, report, heartbeat, early load, anti-detect) |
| Pre-sweep | `MapperBattleyePreMap` + early **`IOCTL 0x852`** after callback strip |
| Bypass bundle | `0x81B` strip, `0x868` NMI, **`0x869`** EAC/BE max, unified `0x827` |
| Payload | `Driver2.sys` — no `PsSetLoadImageNotifyRoutine` at entry; mapper-ordered sweep |

## Usermode API

```cpp
#include "sdk/driver2_api.h"

HANDLE h = Driver2Open();
Driver2Auth(h);
Driver2BattleyeImprove(h, BE_ALL, /* target game pid or 0 */);
Driver2ReadProcess(h, pid, va, buf, size);
```

CLI:

```text
Driver2Client.exe be-improve
Driver2Client.exe probe
```

## Evidence / captures

- **R6:** `dumps/r6-battleye_*` (`BEService_r6s.exe`, `BEClient_x64.dll`)
- **ARK SA:** profile `be-arksa.json` (mapper `arksa`); dump pending — uses same BE IOCTL masks as R6

Verify log: `%TEMP%\pf_diag.tmp` must show **`0x852`**, **`0x869`**, and **callback strip**.

```powershell
.\byovd\scripts\verify-pf-diag.ps1 -Profile r6
```

## Gates

```powershell
.\driver2\developer-approval-gate.ps1 -Execute -ForceMap -Inspector   # includes r6
.\driver2\developer-approval-gate.ps1 -Execute -ForceMap -IncludeBe  # adds arksa
```

## Live ranked (honest)

BE + BYOVD on the game PC is often **Tier 0** (RTCore blocked). Use **Lane D** for live reads; map BE profiles only on **no-AC VM**. See `docs/12_lane_d_split_trust.md`.
