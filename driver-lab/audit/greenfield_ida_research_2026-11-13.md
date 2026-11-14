# Greenfield driver research → IDA (not “known” LOL)

Live Rust/R6 + Win11 + paid AC: **anything on LOLDrivers or the Windows vulnerable-driver blocklist is already dead** for that lane. Mapper lab can still load some of them; that is **VM proof only**, not an ACS strategy.

**They can’t be “known” ahead of time.** The workflow is:

1. **Find who ships a `.sys`** (peripheral RGB, audio, fan tools, motherboard utils, capture cards).
2. **Heuristic pass** (`dumpbin /imports`, strings) — tier ≥2 is “worth IDA,” not “works.”
3. **IDA** — IOCTL table, `CreateFile` gate, does `MmMapIoSpace` / phys / MSR / port I/O reach usermode with weak checks?
4. **VM only** — `sc start` + mapper or custom IOCTL fuzz; never host ranked PC.

## Where to hunt (official sources)

| Category | Examples | Typical download |
|----------|----------|------------------|
| RGB / fans | Lian Li, NZXT CAM, SignalRGB, OpenRGB (needs PawnIO) | Vendor site / winget |
| Mice / keyboards | SteelSeries Engine/GG, Razer Synapse, Roccat Swarm, Wootility | CDN installers |
| Headsets / audio | Corsair iCUE, Logitech G HUB, Turtle Beach Audio Hub, HyperX NGENUITY, Elgato Wave Link | MSI/EXE (often packs virtual audio `.sys`) |
| Motherboard | Gigabyte RGB Fusion, ASUS Armoury Crate (`AsIO3.sys` / `IOMap64.sys` in full pack) | OEM utility zip |
| Monitoring | HWiNFO, FanControl, ThrottleStop | TechPowerUp / GitHub releases |

Avoid treating **GitHub “vulnerable driver” lists** as a live vehicle catalog. Use them only to understand **patterns** (IOCTL shape, `MmMapIoSpace` call sites).

## IDA “will it work?” (minimal bar for mapper-shaped work)

Must eventually satisfy **all** of:

- **Loadable** — WHQL/third-party sign, loads on 26200 in VM (`sc create` / mapper hygiene).
- **Openable** — `\\.\Something` from elevated worker (not hard-coded to vendor EXE only).
- **Primitive** — IOCTL path to arbitrary **physical** read/write or full-page map to user VA (not only GPIO/SMBus to one PCI ID).
- **Walkable** — after CR3 bootstrap, can read kernel MZ / PTEs (ThrottleStop-class **NonCached** PT reads failed here; Corsair **may** differ — prove in IDA + one IOCTL test).

See `driver-lab/docs/17_local_driver_ida_audit.md` for MCP tool sequence.

## Staged for IDA today (`audit/inbox/oem-sys/x64/`)

| File | Why open in IDA | LOL / blocklist note |
|------|-----------------|----------------------|
| `CorsairLLAccess64.sys` | Imports `MmMapIoSpace`, `MmMapLockedPagesSpecifyCache`; IOCTL `0x225374` documented in `primitive_corsair.hpp` | On LOLDrivers — **VM lab only** |
| `PawnIO.sys` | `MmMapIoSpace` + script VM; different device model | Not LOL; gate is **Pawn bytecode**, not raw phys IOCTL |
| `cam_driver_mb.sys` | `MmMapIoSpace` in strings; NZXT motherboard helper | Unknown public status — **best greenfield shape** |
| `ssdevfactory.sys` | `root\ssdevfactory` — generic factory driver (SteelSeries) | Unknown — check IOCTL table |
| SteelSeries `sshid.sys`, `ssps2.sys`, … | HID stack — usually **no** arbitrary phys | Low priority unless IDA finds odd IOCTLs |
| `CorsairGamingAudio64.sys` | Headset path | Audio filter — unlikely full phys map |

**Not extracted yet (install in VM → copy from DriverStore → inbox):** ViGEmBus, Voicemeeter virtual audio, G HUB stack, Razer inner package, Gigabyte RGB Fusion inner `setup.exe`.

## Heuristic imports (staged x64 only)

```
cam_driver_mb.sys            MmMapIoSpace, HalGetBusData
CorsairLLAccess64.sys        MmMapIoSpace, HalGetBusData, MmMapLockedPages
PawnIO.sys                   MmMapIoSpace, HalGetBusData
```

Everything else in the peripheral batch: **skip mapper** until IDA says otherwise.

## Inbox automation

```powershell
cd driver-lab\scripts
.\extract-oem-to-inbox.ps1 -RefreshHeuristics
```

Outputs: `audit\oem_sys_manifest_<date>.csv`, `audit\driver_byovd_heuristic_oem_<date>.csv`. Tier≥2 rows go to IDA first; tier 0 HID drops unless IOCTL surprise.

Fast VM gate (no mapper_nal): `byovd\bin\VehiclePhysProbe.exe corsair` after elevated copy from `bin\`.

## Next concrete steps

1. Copy greenfield picks to `driver-lab/audit/inbox/` and run IDA MCP `survey_binary` + IOCTL decompile on **`cam_driver_mb.sys`** and **`CorsairLLAccess64.sys`** (compare gating).
2. On guest: install **one** unknown vendor suite (e.g. Gigabyte Fusion or G HUB only in VM), run `enumerate-local-drivers.ps1`, inbox new `.sys`, repeat IDA — **no** public hash hunting.
3. VM map **only after** IDA shows weak IOCTL + device open; treat ThrottleStop/RTCore as **negative controls** on 26200.

Mapper note: blind VM runs died early on security heartbeat under Task Scheduler; lab `--no-auth` now pauses heartbeat — still **does not** replace IDA proof.

## IDA verdicts (2026-11-13, sessions `78d6f4b6`, `a375391f`, `62e4fe69`)

| Driver | Open as | LOL / blocklist | Mapper-shaped? | Notes |
|--------|---------|-----------------|----------------|-------|
| `cam_driver_mb.sys` | `\\.\CAMDriverMB` | SHA **not** in local `loldrivers-drivers.csv` | **No** | Rust/WDF NZXT board helper. `MmMapIoSpaceEx` only inside large Nuvoton/ITE probe (`sub_180002CB0`). User path is Super I/O **port I/O** + typed GPIO IOCTLs (`nuvoton_sio_*_gpio.rs`), not arbitrary physical R/W. SDDL: `WD` has `GRGW` — opens wide, primitives narrow. |
| `ssdevfactory.sys` | (WDF USB factory, no `\\.\` string) | not in local LOL CSV | **No** | SteelSeries USB enumerate/install only. No `MmMapIoSpace`, no HAL bus R/W imports. |
| `PawnIO.sys` | `\\.\PawnIO` | not in local LOL CSV | **No (for mapper_nal)** | `MmMapIoSpace` + HAL PCI config, but **RSA/BCrypt + pawn bytecode VM** (`call_native`, `pawnio_trusted_keys`). Phys only via signed scripts, not raw IOCTL. |

**Implication:** none of the three greenfield inbox picks justify extending `mapper_nal` today. Next hunt = **install unseen vendor suite in VM** (Gigabyte Fusion inner setup, G HUB payload, ViGEm) → DriverStore extract → repeat IDA IOCTL table.
