# STOMP bypass — image-backed driver2 (private)

Status: **[proposed]** — not executed on DriverLab VM yet. Target: live EAC/BE/ACE on build 26200+.

## Opsec

- Never upload `Driver2Stomp.sys`, victim profiles, or compiled vehicles to scanners.
- Never publish binaries; rotate victim + payload if suspected.

## Detection-surface vs pool map

| Fingerprint (see `driver2/AC_FINGERPRINTS.md`) | Pool map | Stomp map |
|-----------------------------------------------|----------|-----------|
| BYOVD resident / device open | Tier 0 | One-shot; vehicle stopped before game |
| Unsigned pool / RIP outside module | High | RIP inside signed victim `.text` |
| Custom `\Device\` cheat device | Med–high | Victim's existing device |
| Disk-vs-memory on victim | n/a | Residual — gate in `ApplyStomp` (.text head compare) |

## Live sequence

1. Boot to desktop; AC and game not running.
2. Admin: `driver2\bin\Driver2Runner.exe --mode stomp --victim dam --driver myport --force-map`
3. Cheat opens victim DOS name (`Driver2SetDeviceName` / `BetestSetDeviceOverride`), `0x870` then `0x805`/`0x806`.
4. Vehicle service deleted/stopped per existing mapper hygiene; mapper exits.

## Lab gates (extend cheat-inspector S2)

| Gate | Pass bar |
|------|----------|
| RIP in module | Dump: payload entry RIP inside victim image range |
| No new devices | `\Device\` snapshot unchanged |
| Vehicle gone | Module list + services: vehicle absent |
| Disk compare | On-disk victim hash unchanged |
| Pass-through | Victim-native IOCTLs still succeed |

## Correctness (engine)

- **Xref scan is conservative**: false rel32/abs detections only shrink the dead set, never admit referenced starts. Residual indirect refs → per-victim IOCTL flood + 24h soak (doc §7).
- **Write ordering**: STOMP_DATA is patched in the mapped buffer before one cluster write; thunk + `MajorFunction[14]` after the image write (mid-fail = unhooked victim with extra bytes in dead region).
- **Thunk / AEP**: `/ENTRY:StompThunkEntry` stabilizes `AddressOfEntryPoint` for the E9 displacement; mapper does not execute payload code.
- **Victim open**: `ResolveDriverObjectFromHandle` needs a handle to the victim DOS device (`CreateFileW` with `0` or `GENERIC_READ` suffices for the handle walk).

## Implementation map

- `src/mapper/stomp.{h,cpp}` — enumerate, dead-code proof, placement, patch, verify
- `main_nal.cpp` — `--stomp`, `--victim`, `--stomp-dry-run`
- `driver2/src/Driver2Stomp.c` — lazy-init dispatch hijack payload
- `profiles/stomp-victims.json` — candidate table