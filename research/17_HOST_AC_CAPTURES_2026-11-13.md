# Host anti-cheat captures (session record)

**Edition:** 13 November 2026 | **Host build:** `26200.9974` | **Researcher host:** Win11 Home, own box.

Read-only disk copies; services were not started for capture unless noted. Canonical bundle: [`../ac-captures-host/README.md`](../ac-captures-host/README.md). Staging: `D:\driver-lab-ac-dumps` (fast mirror); lab mirror: `driver-lab/dumps/`, `driver-lab/audit/inbox/`.

## 1. Objectives

| Goal | Outcome |
|------|---------|
| Pin **Rust** to EOS shared driver + game `Settings.json` | Done |
| Capture **R6**, **Delta Force**, **Arena Breakout Infinite** AC stacks | Done (kernel/user; see gaps) |
| **ARK Survival Ascended** | Pending (`BEService_arksa.exe` only; partial game tree) |
| Feed **IDA** + **mapper `--ac-replica`** profiles | `config/ac_host_captures.json` |

## 2. Capture script

`driver-lab/scripts/dump-title-anticheat.ps1`

| Title | Parameter |
|-------|-----------|
| Rust | `rust` (also `dump-rust-eac.ps1`) |
| Rainbow Six Siege | `r6` |
| ARK SA | `arksa` |
| Delta Force | `deltaforce` |
| Arena Breakout Infinite | `arenabreakout` |

**Operational note:** Writing large trees directly under `driver-lab/dumps` on OneDrive **stalled** (multi-minute hang). Fix: default `OutRoot` = `D:\driver-lab-ac-dumps`, then `Sync-DumpToLab` → `driver-lab/dumps`. Run **one title per invocation**.

ACE bundles copy **essentials** only (`.sys`, `.dat`, SGuard `product.rc`) — not 60MB+ plugin DLL trees.

## 3. Per-title inventory

### Rust (Steam) — EasyAntiCheat EOS

| Field | Value |
|-------|-------|
| Game path | `C:\Program Files (x86)\Steam\steamapps\common\Rust` |
| EOS driver | `C:\Program Files (x86)\EasyAntiCheat_EOS\EasyAntiCheat_EOS.sys` |
| Identity | `EasyAntiCheat\Settings.json` → `title: Rust`, `RustClient.exe`, EOS product/sandbox/deployment IDs |
| **SHA256** | `A38FA3D4158FBD4503DC4D4BA303493B25B9DCFEEDB81D4386F2BB71888CBE7C` |
| Size | ~42,884,760 bytes |
| Dump dir | `driver-lab/dumps/rust-eac-eos_2026-11-13_0204` |
| IDA | `driver-lab/reverse/EasyAntiCheat_EOS/bin/`, inbox `audit/inbox/EasyAntiCheat_EOS.sys` |

### Rainbow Six Siege — BattlEye (+ EAC cert channel)

| Field | Value |
|-------|-------|
| Game path | `D:\SteamLibrary\steamapps\common\Tom Clancy's Rainbow Six Siege` |
| BE bundle | `BattlEye\` (client DLL, `BELauncher.ini` → `GameID=r6s`) |
| EAC folder | `EasyAntiCheat\Certificates\` only (runtime channel, not primary AC) |
| Usermode anchor | `C:\Program Files (x86)\Common Files\BattlEye\BEService_r6s.exe` |
| **BEDaisy.sys** | **Not on disk** at capture — install via game/BE before kernel dump |
| Dump dir | `driver-lab/dumps/r6-battleye_2026-11-13_0214` |

### Delta Force — AntiCheat Expert (Tencent)

| Field | Value |
|-------|-------|
| Game path | `D:\Delta Force\Delta Force` |
| ACE bundle | `DeltaForce\Binaries\Win64\AntiCheatExpert\` |
| **SGuard build** | `1.376.2512.2118` (`SGuard\x64\product.rc`) |
| **ACE-GAME (game bundle)** | SHA256 `C3FE27FC11CD1BAECF1CD9E2C63F1FDA4EB61A498D1F74B9FDFDC1FC2D696990` |
| **ACE-GAME (system32)** | SHA256 `2B6DB055B37EC90CAC2C62153432BE47CEEA3718B55C0FE400BF9665B61F46E4` |
| **kernel_match** | **false** — static/IDA use **game bundle** |
| Dump dir | `driver-lab/dumps/deltaforce-ace_2026-11-13_0215` |
| IDA inbox | `ACE-GAME_deltaforce.sys` → `reverse/ACE-GAME_deltaforce/` |

### Arena Breakout Infinite — ACE

| Field | Value |
|-------|-------|
| Game path | `D:\Arena Breakout Infinite` |
| ACE bundle | `ABInfinite\Binaries\Win64\AntiCheatExpert\` |
| **SGuard build** | `1.369.2511.2095` |
| **ACE-GAME** | SHA256 `2B6DB055B37EC90CAC2C62153432BE47CEEA3718B55C0FE400BF9665B61F46E4` |
| **kernel_match** | **true** (bundle = system32 at capture) |
| Dump dir | `driver-lab/dumps/arenabreakout-ace_2026-11-13_0215` |
| IDA inbox | `ACE-GAME_arenabreakout.sys` |

### ARK Survival Ascended — BattlEye (incomplete)

| Field | Value |
|-------|-------|
| Game path | `D:\SteamLibrary\steamapps\common\ARK Survival Ascended` |
| Install state | No `ShooterGame*.exe` in tree at scan — likely partial install |
| Usermode anchor | `BEService_arksa.exe` in Common Files\BattlEye |
| In-game `BattlEye\` | Not present |
| Mapper profile | `arksa` wired same BE masks as `r6`; dump script ready |

### System32 ACE stack (reference, not title-specific)

Present at capture: `ACE-GAME.sys`, `ACE-BASE.sys`, `ACE-ADVT.sys`, multiple `ACE-CORE*` services. `ACE-GAME` on disk matched **Arena Breakout** bundle, **not** Delta Force game bundle.

## 4. Services snapshot (EAC check task)

`EasyAntiCheat_EOS` service was **Stopped** during one probe; capture is **from disk**, valid for Rust identity.

## 5. Related paths

| Artifact | Path |
|----------|------|
| Bundled copy + driver how-to | `ac-captures-host/` |
| Profile JSON | `byovd/docs/ac_host_captures.json`, `ac_replica_profiles.json` |
| LOLDrivers / policy | `driver-lab/data/`, `byovd/docs/ac_eac_be_vehicle_policy.json` (`paid_ace` tier added) |

## 6. Open follow-ups

1. Run R6 (or `Install_BattlEye.bat`) once → re-dump `BEDaisy.sys` into R6 bundle.
2. Complete ARK install → re-run `dump-title-anticheat.ps1 -Title arksa`.
3. Chunked IDA on `EasyAntiCheat_EOS.sys` + title `ACE-GAME` clones (dispatch / `IRP_MJ_DEVICE_CONTROL`).
4. Optional: bundle `EOSSDK-Win64-Shipping.dll` for Rust usermode parity.
