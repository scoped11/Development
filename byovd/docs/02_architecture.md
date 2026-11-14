# How the map and bypass surface work

Two layers: **vehicle** (signed vulnerable driver) and **payload** (unsigned Betest, pool-mapped).

```
usermode mapper_nal.exe
    │  loads vehicle.sys as a Windows service (Nal / Trixx / …)
    │  uses vehicle IOCTL for kernel R/W + pool alloc
    ▼
ManualMap(Betest.sys) → NonPaged pool (relocs, IAT, NX cleared on pages)
    │  HAL dispatch trampoline + NtQueryIntervalProfile
    ▼
Betest DriverEntry(NAL's real DRIVER_OBJECT)
    │  IoCreateDevice + symlink (dynamic name)
    │  MajorFunction[IRP_MJ_DEVICE_CONTROL] = Betest
    ▼
usermode overlay / examples/process_rw
    DeviceIoControl → process/kernel R/W
```

## Why a real DRIVER_OBJECT

A fake `DRIVER_OBJECT` with `TypeIndex == 0` crashes in `IoCreateDevice` (object manager cookie). Mapper enumerates `SystemExtendedHandleInformation` for `\\.\Nal` → `FILE_OBJECT` → `DEVICE_OBJECT` → **real** `DRIVER_OBJECT`, then runs `DriverEntry` with that pointer.

## Why we do not STOP the vehicle after map

Betest’s device and dispatch live on the vehicle object. `ControlService(STOP)` frees it → use-after-free BSOD. After `g_mapSucceeded`, cleanup **does not stop** the service. Reboot to unload.

## Bypass surface (still in the driver)

`BetestDriver.c` still contains the full CTF IOCTL table (callbacks, ETW, DKOM, CR3, VGK, BattlEye/EAC “improve”, deep hide, …). **Nothing was deleted.**

| When | What runs |
|---|---|
| **Always on DriverEntry** | Device + symlink, `MmCopyVirtualMemory` resolve, image-load **notify register**, **record** BE/EAC/VGK module ranges |
| **Default map (RW-only)** | Mapper does **not** send strip/kill/unified/DKOM/mutate |
| **`--eac` / `--battleye` / `--ace` / `--deep` / …** | Mapper still sends **those** IOCTLs if you pass the flags |
| **`--ac-preempt` or `--full`** | Automatic preempt block (strip all, kill timers/threads, unified neut, later DKOM/PG_SAFE/MUTATE) |
| **`g_allowAutoAcNeutralize`** | Starts **0**. Set when disable / strip / unified IOCTLs run. Until then, `OnImageLoad` **does not** patch AC `DriverEntry` |

So: **R/W works without fighting AC.** Fighting AC is opt-in and is the main remaining BSOD / PatchGuard source.

## Payload residence

Betest executes from **non-image-backed pool**, not from a `PsLoadedModuleList` PE. That is the “suspicious RWX region” in kdmapper/stomp writeups. **Default map is still pool.** Optional **image-backed stomp** (`Driver2Stomp.sys`, `mapper_nal --stomp`) is documented in `docs/STOMP_BYPASS.md` — lab only until victim gates pass.

## Communication fingerprint

A new `\Device\` + large IOCTL function range (`0x800+`) is enumerable. Literature alternative: hijack an existing driver’s `IRP_MJ_DEVICE_CONTROL` / XFG chokepoint. Not implemented here (stability + fingerprint tradeoff).

## Source map

| Piece | Path |
|---|---|
| Mapper + HAL trampoline | `src/mapper/main_nal.cpp` |
| PE map | `src/mapper/mapper.cpp` |
| Vehicle primitives | `src/mapper/primitive_*.hpp` |
| Payload + IOCTLs | `src/driver/BetestDriver.c` |
| Usermode contract | `include/betest_ioctl.h` |
