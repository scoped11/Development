# STOMP bypass — image-backed driver2 (private)

Status: **[in-tree, lab]** — engine + `Driver2Stomp.sys` build; **dry-run / live stomp on DriverLab-E** (guest ~26300). Live EAC/BE/ACE on build 26200+ **not gated** until cheat-inspector S2 passes on guest.

## Opsec

- Never upload `Driver2Stomp.sys`, victim profiles, or compiled vehicles to scanners.
- Never publish binaries; rotate victim + payload if suspected.

## Detection-surface vs pool map

| Fingerprint (see `driver2/AC_FINGERPRINTS.md`) | Pool map | Stomp map |
|-----------------------------------------------|----------|-----------|
| BYOVD resident / device open | Tier 0 | One-shot; vehicle stopped before game (`g_stompKeepVehicle` skips unload on live stomp only during map) |
| Unsigned pool / RIP outside module | High | RIP inside signed victim `.text` |
| Custom `\Device\` cheat device | Med–high | Victim's existing device |
| Disk-vs-memory on victim | n/a | Residual — gate in `ApplyStomp` (.text head compare) |

## Build & paths

| Artifact | Host | Guest (share) |
|----------|------|----------------|
| `mapper_nal.exe` | `byovd\bin\`, `byovd\driver2\bin\` | `Z:\byovd\driver2\bin\` (mirror via `sync-driver2-bin.ps1`) |
| `Driver2Stomp.sys` | same | same |
| WDK | **10.0.26100.0** | — |

Rebuild on host after stomp changes; confirm guest **LastWriteTime** on `mapper_nal.exe` before trusting dry-run timing.

## Mapper flow (early stomp)

With `--stomp` / `--stomp-dry-run`, the mapper runs **immediately after BYOVD smoke test** (kernel R/W verified). It **does not** run cave scan, HAL dispatch, NX warm, or pool `ManualMap` first. Without stomp flags, behavior is unchanged (full bootstrap).

CLI (guest admin):

```text
mapper_nal.exe Driver2Stomp.sys --stomp --stomp-dry-run --victim luafv --driver myport --no-auth --quiet --force-map --no-fallback
```

Runner equivalent:

```text
Driver2Runner.exe --mode stomp --victim luafv --dry-run --driver myport --skip-rw
```

Dry-run prints **STOMP plan** (dead regions, cluster RVA, thunk, fit) and exits without writing the victim.

## VM lab order (mock AC **after** driver)

Per `byovd/docs/15_test_ac_replicas.md`: mock EAC/BE must be **stopped** before map (Tier 0). Lab pipeline:

1. `guest-stop-ac-replica.ps1`
2. `guest-stomp-live-map.ps1` (`wcifs` on DriverLab-E as of 2026-11-14)
3. `Driver2Client.exe --device \\.\WCIFS probe` (cheat before AC)
4. `Start-AcReplicaServices.ps1 -Profile rust|r6`
5. `Driver2Client.exe` probe again

Orchestrator: `driver-lab/vm/bg/guest-signed-module-replica.ps1` (appearance gate: disk SHA-256 and Authenticode stay the victim’s). `guest-stomp-lane-full.ps1` only forwards to it. **No** Authenticode graft onto another `.sys`. Reboot guest after **0x3B** before the next live map. Doc: `driver-lab/docs/31_signed_module_replica.md`.

## Live sequence (target)

1. Boot to desktop; AC and game not running.
2. Admin: `driver2\bin\Driver2Runner.exe --mode stomp --victim <name> --driver myport --force-map` (or `mapper_nal` as above without `--stomp-dry-run`).
3. Cheat opens victim DOS name (`Driver2SetDeviceName` / `BetestSetDeviceOverride`), `0x870` then `0x805`/`0x806`.
4. Vehicle service deleted/stopped per existing mapper hygiene; mapper exits (live stomp may keep vehicle mapped per `g_stompKeepVehicle`).

Victims table: `profiles/stomp-victims.json`. On DriverLab-E **2026-11-14** hijack probe: **`luafv.sys` / `wcifs.sys`** present; **`dam.sys`** absent on that image — prefer **luafv** for first guest stomp. CSV: `driver-lab/audit/hijack_candidates_vm_2026-11-14_0855.csv`.

## Lab gates (extend cheat-inspector S2)

| Gate | Pass bar |
|------|----------|
| RIP in module | Dump: payload entry RIP inside victim image range |
| No new devices | `\Device\` snapshot unchanged |
| Vehicle gone | Module list + services: vehicle absent (or documented stomp keep-vehicle policy) |
| Disk compare | On-disk victim hash unchanged |
| Pass-through | Victim-native IOCTLs still succeed |

## Correctness (engine)

- **Xref scan is conservative**: false rel32/abs detections only shrink the dead set, never admit referenced starts. Residual indirect refs → per-victim IOCTL flood + 24h soak.
- **Write ordering**: STOMP_DATA is patched in the mapped buffer before one cluster write; thunk + `MajorFunction[14]` after the image write (mid-fail = unhooked victim with extra bytes in dead region).
- **Thunk / AEP**: `/ENTRY:StompThunkEntry` stabilizes `AddressOfEntryPoint` for the E9 displacement; mapper does not execute payload code.
- **Victim open**: `ResolveDriverObjectFromHandle` needs a handle to the victim DOS device (`CreateFileW` with `0` or `GENERIC_READ` suffices for the handle walk).
- **Primitives**: `KernelPrimitives::ReadKernelMemory` / `WriteKernelMemory` from `mapper.hpp` (not a separate `stomp::Primitives` type).

## DriverLab-E worker (host queues guest)

Share: `Z:\byovd`, worker: `driver-lab/vm/bg/`.

| Script | Role |
|--------|------|
| `host-bg-admin.ps1 -Action exec -PowerShell "…"` | Queue elevated exec; host polls `vm/bg/out/<id>.json` |
| `guest-kill-stuck-mapper.ps1` | Kill `mapper_nal`, stop vehicle services, related guest scripts |
| `guest-stomp-plan-only.ps1` | Guest `stomp_plan.exe` (seconds, no vehicle) |
| `guest-stomp-dryrun-watchdog.ps1` | BUILD stamp → `mapper_nal --stomp-dry-run` (user-only, no vehicle open) |
| `guest-signed-module-replica.ps1` | Disk identity gate + live stomp + L2 replica + client probe |
| `guest-stomp-lane-full.ps1` | Wrapper for `guest-signed-module-replica.ps1` |
| `guest-lab-preflight.ps1` | Secure Boot + testsigning gate for offline vault jobs (exit **126**) |
| `misc/stomp_plan.ps1` + `bin/stomp_plan.exe` | Host-only placement before guest map |

See `driver-lab/vm/bg/README.md` for hangs/timeouts.

### Live stomp victim blocklist (lab)

Do **not** stomp `fltmgr`, `ndis`, `disk`, etc. — live patch on **fltMgr** caused **0x3B** on DriverLab-E (2026-11-14). Use only profile candidates (`luafv`, `wcifs`, `dam`) once `stomp_plan` passes. Never live-stomp anticheat modules (plan-only ok).

### “Stuck” vs BSOD (lab)

| Signal | Hung mapper (common) | BSOD |
|--------|----------------------|------|
| `worker.heartbeat` updates | Yes | Often stops until reboot |
| Other worker jobs complete | Yes (when not blocked) | No |
| `guest-kill-stuck-mapper` helps | Often | N/A after reboot |
| New `Minidump\*.dmp` | No | Yes |

Typical causes: **serial worker** (one long `mapper_nal` blocks the queue); **stale method-lane** `mapper_nal` (`vm/bg/map-progress.txt`); **pre–early-stomp** mapper doing full cave/NX scan; host **timeout** (`exit 2`) while guest exec still running. **Not** consistent with guest reboot.

## Supplemental CI / attestation

Force-trusted test sign on guest still fails **`0xc0000428`** / `sc start` 577 — **deprioritized**. Long-term load without BYOVD: `docs/SIGNING_PATH.md`. 25H2 BYOVD is **blocklist / cert hash**, not “unsigned refused by OS.”

## Implementation map

- `src/mapper/stomp.{h,cpp}` — enumerate, dead-code proof, placement, patch, verify
- `src/mapper/main_nal.cpp` — `--stomp-dry-run` exits **before** vehicle open (`ExecuteStompDryRunUserOnly`); live stomp still runs after smoke; `g_stompKeepVehicle`
- `PlanPlacement` — dead cluster → **longest `.text` CC run** → high-water tail; `stomp_plan` prints `max_cc` on failure
- **Sizing gate:** on host **26200**, `luafv` / `wcifs` fail (`need=0x7000`, `max_cc≈0x7c`) — run `stomp_plan` on **guest** probe winners or shrink `Driver2Stomp` before live map
- `driver2/src/Driver2Stomp.c` — lazy-init dispatch hijack; `.stompdata` contract (+0x00 orig, +0x08 `fnGetRoutine`, +0x10 `MmCopyVirtualMemory`, +0x18 sweep, +0x1C `STM2`)
- `driver2/Driver2Runner.cpp` — `--mode stomp`
- `include/betest_ioctl.h` — `BetestSetDeviceOverride` / victim DOS open
- `profiles/stomp-victims.json` — candidate table