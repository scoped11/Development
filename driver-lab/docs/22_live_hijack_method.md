# Live method (no DMA): dispatch hijack

**Constraint:** Rust/EAC or R6/BE **up**. No DMA. No ACE. No new LOL `.sys`. No SI.

**Pick: hijack.** A 0-day is only the **kernel R/W plug**, not the live architecture.

```text
[0-day / 1-day]  →  kR + kW on this build
        ↓
Find DRIVER_OBJECT of a module ALREADY loaded in the match
  (not EasyAntiCheat_EOS / BEDaisy as first host — those are integrity-watched)
        ↓
Overwrite MajorFunction[IRP_MJ_DEVICE_CONTROL]  (see XFG chokepoint below)
        ↓
Betest-equivalent IOCTLs on THAT device — no IoCreateDevice, no new symlink
```

This is the row already called out as unimplemented in `byovd/docs/02_architecture.md` and `04_detection_surfaces.md`.

## XFG chokepoint variant (two writes, in-image dispatch)

Public write-ups describe routing `IRP_MJ_DEVICE_CONTROL` through an existing **in-image** CFG/XFG helper instead of pointing `MajorFunction[14]` straight at pool/shellcode:

```text
DeviceIoControl → MajorFunction[14] → _guard_xfg_dispatch_icall_nop (in host .text)
                                          → jmp [__guard_dispatch_icall_fptr]  (.data)
                                          → handler (prefer same image as host / stomp target)
```

| Stage | What changes | Why |
|-------|----------------|-----|
| 1 | `MajorFunction[IRP_MJ_DEVICE_CONTROL]` → RVA of `_guard_xfg_dispatch_icall_nop` | Integrity sees dispatch **inside** signed host `.text` |
| 2 | `__guard_dispatch_icall_fptr` → handler | Avoids `.text` trampoline; indirect call path |

**Not magic:** `DRIVER_OBJECT` lives in pool — stage 1 is still a kernel write. EAC can compare `MajorFunction[14]` to boot baseline, audit the fptr, walk the indirect target, and still hit NMI/unbacked-PC checks. Stomp/module-compare is a **separate** layer (optional; not required for H2 on VM if handler is already in the host image or a known cave).

**Out of scope for live (this repo):** kdmapper/iqvw64e load, PiDDB/MmUnloadedDrivers scrub, pool payload + stomp pipeline, win32k session-space `.data` syscalls (`NtUser*` via `win32u.dll`). Those are alternate comm channels and BYOVD lifecycle — we use **kR/kW primitive + existing `\\.\` device**.

## Why hijack instead of “just 0-day”

| 0-day alone | Hijack |
|-------------|--------|
| Gives kW | Makes kW **usable as the mapper** |
| Still need somewhere for RIP / IOCTL | Reuses a **signed, already-loaded** dispatch |
| New pool device = loud | No new `\Device\Betest` |

You still **need** a primitive. Hijack does not replace the 0-day; it **consumes** it.

## Live host rules

| Do | Don’t |
|----|--------|
| Hijack a **non-AC** resident driver (GPU/storage/filter you can open) | Load RTCore / Nal / ACE-GAME |
| Keep payload **in-image** of the host if possible (stomp) or accept pool + hijack | `IoCreateDevice` on a fake object |
| One IOCTL range, RW-only | `--full` / strip EAC on first live try |

## Plug the 0-day (you bring this)

Mapper already talks to vehicles via `primitive_*.hpp` (`RtcPhysRead` / write, etc.). Live path is the **same contract** without `CreateService`:

- `KRead(va, buf, n)` / `KWrite(va, buf, n)`  
- Then existing: resolve `DRIVER_OBJECT` from an **already open** `\\.\…` handle (`SystemExtendedHandleInformation` — same as Nal today)  
- Then: **patch that object’s dispatch**, not `DriverEntry(vehicle)`.

No CVE, no PoC, no patch bytes in this repo. Fill `primitive_id` in `lab_manifest.lane_e` when you have a private primitive.

## Acceptance (live)

| Level | Pass |
|-------|------|
| H0 | kR/kW works on E-VM (`lane_e` E1) |
| H1 | `DRIVER_OBJECT` of a **pre-loaded** host resolved without `sc create` |
| H2 | DeviceIoControl to **existing** device runs payload handler (no new symlink) |
| H3 | Same on E-AC with game in menu — kick/crash/silent recorded |

H3 is “live method works.” Nothing before H2 is live.

## What we implement in-tree vs not

| In tree | Not in tree |
|---------|-------------|
| This method + manifest `lane_e` | 0-day source |
| Later: `--no-load --device \\.\Host` **wiring** once primitive exists | IRP patch recipes, EAC/BE dispatch offsets |

## Bottom line

**Live method = hijack an already-loaded driver’s dispatch, fed by a 0-day you do not check in.** DMA is out. BYOVD load is out. Until kW exists, the mapper cannot complete H2 — the work is **primitive + hijack host pick**, not another greenfield `.sys`.
