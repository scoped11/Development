# Existing bypass / stealth surface (already in tree)

You asked for **more** bypasses. The payload + mapper already ship a **large CTF IOCTL set** — nothing was removed. This file catalogs what exists and how to invoke it on an **isolated lab VM**. It does not claim undetectability on live EAC/BE titles.

**Start with the recipe index:** [14_eac_cookbook.md](14_eac_cookbook.md) (lanes, gates, R1–R6). Use **this file** as the IOCTL/flag appendix when a cookbook recipe points you at Tier 3 experiments.

## Mapper modes (main_nal.cpp)

| CLI | What it does | BSOD / PG risk |
|---|---|---|
| *(default)* `--rw-only` | Map only; skip **automatic** preempt strip/DKOM/mutate | Low |
| `--ac-preempt` | Preempt strip, kill timers/threads, unified neut, exit DKOM/PG/mutate | **High** |
| `--full` | Sets all feature flags + disables rw-only | **Very high** |
| `--eac <hex>` | `IOCTL_EAC_IMPROVE` feature mask | High |
| `--battleye <hex>` | BattlEye improve IOCTL | High |
| `--ace <hex>` | ACE strengthen | High |
| `--adv` / `--final` / `--deep` | ACE hardening stacks | High |
| `--delta` / `--apex` | Title-specific hardening IOCTLs | High |
| `--stealth <hex>` | Mapper usermode + post-map stealth helper | Medium |
| `--dbg <hex>` | DeepHide, PG_RESIST, KillTelemetry (driver) | **PG** |
| `--dkom-*` | Process/thread/driver DKOM flags | **PG** |
| `--hwid*` / `--tpm-spoof` | HWID / TPM lab IOCTLs | Policy / stability |
| `--no-cleanup` | Skip failed-map service cleanup | — |

Example (lab only, not default):

```text
mapper_nal.exe bin\Betest.sys --driver nal --eac 3f --battleye 1f
mapper_nal.exe bin\Betest.sys --driver nal --ac-preempt
```

## Driver IOCTL groups (BetestDriver.c)

Kernel R/W (safe-ish on lab targets):

- `0x800–0x806` — safe storage, kernel R/W, **process R/W** (see `01_read_write.md`)
- `0x870` — session auth

Anti-cheat oriented (already implemented; opt-in):

| IOCTL area | Hex range (approx) | Purpose |
|---|---|---|
| Disable BE/EAC/ACE callbacks | `0x807`, `0x80A`, `0x80B` | Callback neutralize |
| Strip all / inline hook | `0x81B`, `0x81C` | Broad callback strip |
| NMI callback neutralize | `0x868` | `KeRegisterNmiCallback` list → passthrough for AC ranges; mapper `--nmi` |
| EAC/BE max-tier **test** | `0x869` | IAT stubs + neutralize + NMI + **drops our image-load notify** |
| MmUnloadedDrivers name clear | mapper hygiene | Pattern walk, zero matching names |
| Service registry delete (no STOP) | mapper hygiene | Key gone, vehicle stays loaded |
| FullDllName + DriverName length 0 | mapper hygiene | With BaseDllName |
| Kill AC timers/threads | `0x815`, `0x81A` | Timer/thread cleanup |
| Unified neutralize | `0x827` | Combined BE/EAC/ACE/VGK scan |
| VGK / HV / MSR | `0x820–0x824` | Vanguard-style paths |
| Deep hide / cleanup / mutate | `0x82A`, `0x82D`, `0x832` | Hiding + signature mutate |
| PG resist / HV evasion | `0x837`, `0x838` | PatchGuard-sensitive |
| EAC improve / status | driver defines | Mapper `--eac` |

Full `#define` list: `src/driver/BetestDriver.c` lines ~52–130+.

## Monitoring drivers vs BYOVD vehicles (e.g. System Informer)

| Class | Example | Kernel primitive | How you call it | Paid EAC / BE on Rust–R6 |
|-------|---------|------------------|-----------------|---------------------------|
| LOL BYOVD | RTCore64, Nal | Phys / MSR / weak IOCTL | `CreateFile` + `DeviceIoControl` | **Block expected** (`ac_eac_be_vehicle_policy.json`) |
| Signed monitor / KPH | SystemInformer.sys | `MmCopyVirtualMemory` / `MmCopyMemory` | **Flt communication port** + BCrypt client | **Not a mapper swap**; high block / ban risk if loaded with game |

IDA + static probes: `driver-lab/reverse/SystemInformer/notes/PASS2_gate_fingerprint.md`, script `driver-lab/scripts/probe-systeminformer-static.ps1`.

## What is **not** in this kit (kdmapper literature)

| Technique | Status |
|---|---|
| PiDDBCacheTable scrub | Not in mapper — see `reference/kdmapper` |
| CI hash bucket | Not implemented |
| WdFilter runtime lists | Not implemented |
| Module stomp + pool wipe | **Lab** — `STOMP_BYPASS.md`, `--stomp` on `mapper_nal` |
| IOCTL hijack on signed driver only (no new device) | Not implemented |
| Vehicle `BaseDllName.Length = 0` | Not implemented (and vehicle stays loaded after map anyway) |

Adding those is a **separate engineering project** with version fragility and residual detection (disk compare, dispatch audit). See `04_detection_surfaces.md`.

## Why we are not adding “more” here

New kernel patches / hooks / list scrubbers:

1. Do not guarantee EAC/BE bypass (server, behavior, usermode AC still run).
2. Increased PatchGuard and crash rate on your host.
3. Are exactly what this documentation set treats as **lab forensics**, not production cheat features.

**Next research step:** run Lab I (`03_research.md`), pick **one** artifact from `04_detection_surfaces.md`, reproduce on a dump, then decide if a **flag-gated** experiment is worth building — not stacking every IOCTL at once.
