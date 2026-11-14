# BYOVD vehicles (you supply the `.sys`)

A **vehicle** is a **legitimately signed** Windows kernel driver that has a **known vulnerability** (usually an IOCTL that reads/writes arbitrary kernel memory). The mapper does **not** load your payload through normal `sc start` + DSE for Betest/RwTest. It:

1. Starts the **vehicle** as a service (so Windows loads a **trusted** `.sys`).
2. Uses the vehicle’s **exploit primitive** (e.g. `\\.\Nal`) to allocate pool memory, patch the kernel, and run **your** payload’s `DriverEntry` on the vehicle’s real `DRIVER_OBJECT`.
3. After map, your tool talks to **your payload’s device** (`\\.\MsFlt3`, etc.), **not** to NAL IOCTLs for game/RW work.

Think: **vehicle = ladder in**; **payload (Betest / RwTest) = what you actually use**.

This repo **does not ship** vehicle files (they are third-party binaries and often blocklisted). Copy them here yourself for lab use only.

## Supported vehicles

| File you copy in | `--driver` | Usermode device |
|---|---|---|
| `NalDrvIntel.sys` / `iqvw64e.sys` / `NalDrv.sys` | `nal` | `\\.\Nal` |
| `TRIXX.sys` | `trixx` | (Sapphire utility) |
| `ThrottleStop.sys` | `ts` | ThrottleStop |
| `SIVX64.sys` | `siv` | SIV |
| `RTCore64.sys` | `rtcore` | `\\.\RTCore64` |
| `CorsairLLAccess64.sys` | `corsair` | `\\.\CorsairLLAccess` |
| `MsIo64.sys` | `msio` | `\\.\MsIo` |
| `MyPortIO_x64.sys` | `myport` | `\\.\MyPortIODev` |

Mapper search order: `vehicles\`, `bin\`, `%TEMP%`, folder containing `mapper_nal.exe`.

## How to “add” a vehicle

1. Obtain the `.sys` from **your** lab source (OEM utility install, old driver package, etc.).
2. Copy into `byovd\vehicles\<name>.sys`.
3. Map with the matching flag, e.g. `bin\mapper_nal.exe bin\RwTest.sys --driver corsair` or `--driver msio`.

We **cannot** check in `NalDrvIntel.sys` for you; Cursor/git should not redistribute vulnerable signed drivers.

## If a vehicle is blocked by anti-cheat

Follow the tiered plan: `docs/11_ac_block_vs_hide_plan.md` (preflight script + `--ac-safe` map).

Quick gate:

```powershell
.\scripts\ac-preflight.ps1 -Driver rtcore
```

## If NAL / iqvw64e is “blocked”

Blocking usually means one or more of:

| Layer | What happens |
|---|---|
| **Disk / ASR** | Policy blocks **writing** or **loading** a known-bad driver file |
| **Anti-cheat** | Blocks `CreateFile("\\.\Nal")`, service create, or loads a deny list at boot |
| **CI / blocklist** | `sc start` fails or driver fails signature/policy checks |

**Hiding Nal after it is already loaded** is not the same as “not being blocked”:

- While the payload is active, **Nal must stay loaded** — stopping the service frees the `DRIVER_OBJECT` your payload hijacked → **BSOD** (`docs/02_architecture.md`).
- Traces you can **reduce in lab** (mapper + in-progress RwTest trace IOCTLs): **PiDDB** load history, **CI hash bucket** list, **DKOM** unlink from `PsLoadedModuleList`, zero **BaseDllName.Length** so a future unload might not land in **MmUnloadedDrivers**, delete temp copies of `.sys`, scrub service **registry** keys **without** stopping the running driver.
- Traces you **cannot** fully erase while R/W works: the vehicle’s **code and pool** are still in kernel; **object manager** still has the device; AC may still see **loaded** vulnerable driver behavior.

**Practical lab path when `nal` is blocked:**

1. Try another vehicle: `--driver corsair`, `rtcore`, `ts`, `trixx`, `siv` (see table).
2. Run map **before** the game/AC starts (mapper `--auto` only configures IOCTLs; it does not bypass block policy).
3. Use a **VM snapshot** with no AC for IOCTL/RW bring-up; measure traces with your manifest (`docs/06_better_research_method.md`).
4. For RwTest: `map-secure-rwtest.ps1` / `run-test-rwtest.cmd` (BYOVD only, no test-signing).

## Quick map examples

```text
copy vehicles\NalDrvIntel.sys bin\
bin\mapper_nal.exe bin\RwTest.sys --driver nal --no-auth --stealth 3f --rw-only
```

```text
copy vehicles\CorsairLLAccess64.sys bin\
bin\mapper_nal.exe bin\RwTest.sys --driver corsair --no-auth --rw-only
```

After map: `bin\process_rw.exe` (opens blend device name + session auth).

See `reference/TRACE_SURFACES.md` for what this kit scrubs vs what kdmapper literature describes.
