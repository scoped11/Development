# driver2 — built for everything

**Ranked Rust / R6 (one PC, driver + EXE):** **[`RANKED_READY.md`](RANKED_READY.md)** · `ranked-ready.ps1 -Execute -Live`

**Split-trust DMA (no kernel on game PC):** [`../driver3/README.md`](../driver3/README.md) · `set-live-read-method.ps1 -Method dma`

**Cheat dev entry:** [`DEVELOPER_SDK.md`](DEVELOPER_SDK.md) · [`ACE_INTEGRATION.md`](ACE_INTEGRATION.md) · [`EAC_INTEGRATION.md`](EAC_INTEGRATION.md) · [`BE_INTEGRATION.md`](BE_INTEGRATION.md) · `profiles/`

**One product** for the full BYOVD lab stack: **direct kernel driver IOCTL access** (like typical cheat usermode↔driver), **hygiene + IOCTL matrix + bypass sweep + forensics**, all titles + **`universal`**, **Driver2.sys** (separate folder from `src\driver\`).

**Direct driver access (after map):** `Driver2Client.exe` + `sdk/driver2_api.h` — process R/W, kernel `kread`, full payload IOCTL table. See **`CAPABILITIES.md`**.

| Piece | Role |
|-------|------|
| `Driver2.sys` | Pool payload (`driver2\src\Driver2Build.c` → shared Betest body, **driver2** DriverEntry policy) |
| `Driver2Runner.exe` | Map + marker read |
| `mapper_nal --driver2` | IOCTL + bypass + hygiene (symlink **kept** by default for dev comm) |
| `everything.ps1` | prove → ioctl → bypass → forensics (**no map without `-Execute`**) |
| **`vm-lab.ps1`** | **DriverLab VM: plan, map, probe, sync** (one file — host + guest) |
| `dumps/` | **EAC + BE + ACE** captures next to `profiles/` (`sync-ac-dumps.ps1`) |

**Lab VM first.** I will **not** live-map until **you** say go — scripts default to **plan only**.

---

## Wait for VM test (your approval)

```powershell
# Plan only — safe to run anytime
.\driver2\vm-test.ps1 -Profile rust -ForceMap

# When you say go (DriverLab guest, Admin, clean snapshot):
.\driver2\vm-test.ps1 -Profile both -Execute -ForceMap
```

---

## Ranked play (everything on **this** machine)

```powershell
# Reads only, no kernel map (Lane D Host A):
.\driver2\ranked-play.ps1 -DmaOnly

# Plan — shows steps, no map:
.\driver2\ranked-play.ps1 -Profile rust

# When you explicitly approve live map on ranked box (high risk):
.\driver2\ranked-play.ps1 -Profile rust -Execute -ForceMap
```

`everything.ps1` runs:

1. Preflight (+ lane-d on ranked target)  
2. **prove** (RwTest) — optional `-SkipProve` on ranked  
3. **ioctl** (Driver2 + title matrix)  
4. **bypass** (full driver2 sweep)  
5. **forensics** → `driver2\forensics\<timestamp>\snapshot.json`

---

## Build

```powershell
cd byovd
.\build.ps1
.\driver2\build.ps1
```

---

## Confidence 85–90%

After **you** run VM test with `-Execute`: `realtime-gate.ps1` → `confidence.json`. See `CONFIDENCE.md`.

Spec: `driver2_everything.json`.

## Developer approval (≥ 90%)

Rubric: **`DEVELOPER_APPROVAL.md`**.

```powershell
.\driver2\cheat-inspector.ps1                       # buyer static bar (no map)
.\driver2\developer-approval-gate.ps1              # plan only
.\driver2\cheat-inspector.ps1 -Execute -ForceMap    # VM when you say go
```

Pass → `developer_approval.json` with `"approved": true` (lab SDK bar — **not** live ban immunity).
