# AC captures + driver lab bundle

Self-contained workspace copy under `Development\ac-captures-host\` — **host anti-cheat dumps**, **IDA inbox drivers**, and **how to use the BYOVD stack** (`byovd`).

**Start here:** [docs/DRIVER_LAB_GUIDE.md](docs/DRIVER_LAB_GUIDE.md) — build, map, session auth, process read/write, hooks, AC profiles, BSOD safety.

---

## Layout

| Path | Contents |
|------|----------|
| **dumps/** | Per-title trees + `manifest.json` (rust EAC EOS, r6 BE, deltaforce ACE, arenabreakout ACE) |
| **inbox/** | `EasyAntiCheat_EOS.sys`, `ACE-GAME_deltaforce.sys`, `ACE-GAME_arenabreakout.sys` |
| **config/** | `ac_host_captures.json`, `ac_replica_profiles.json` (mapper `--ac-replica` recipes) |
| **sdk/** | `betest_ioctl.h` — IOCTL codes + `BetestOpenDevice` / read/write helpers |
| **examples/** | `process_rw.cpp` — minimal authenticated read test |
| **docs/** | Driver guide + architecture + R/W + IOCTL catalog + Lane E hijack note |
| **MANIFEST.json** | Bundle index + inbox SHA256 |

Fast staging mirror (optional): `D:\driver-lab-ac-dumps\` (same dump folder names as under `dumps/`).

Live source tree: `C:\Users\baile\OneDrive\Desktop\Development\byovd\`  
IDA clones: `Development\driver-lab\reverse\` (e.g. `ACE-GAME_deltaforce`, `EasyAntiCheat_EOS`).

---

## Quick commands

```powershell
# Build drivers + mapper
cd ..\byovd
.\build.ps1
copy vehicles\RTCore64.sys bin\

# Safe R/W proof (VM / no AC)
.\scripts\map-ac-replica-lab.ps1 -Profile rust -Mode prove -Driver rtcore
..\byovd\bin\process_rw.exe

# Title IOCTL lab (snapshot)
.\scripts\map-ac-replica-lab.ps1 -Profile deltaforce -Mode ioctl -Driver rtcore
```

---

Captured **2026-11-13** on host build **26200.9974**. Read-only archives; does not start/stop game AC services.

**Research narrative (Markdown):** [`../research/17_HOST_AC_CAPTURES_2026-11-13.md`](../research/17_HOST_AC_CAPTURES_2026-11-13.md) through [`20_SESSION_DRIVER_OPS_AND_STABILITY.md`](../research/20_SESSION_DRIVER_OPS_AND_STABILITY.md) — indexed in [`../research/00_README_RESEARCH_ADDENDUM.md`](../research/00_README_RESEARCH_ADDENDUM.md).
