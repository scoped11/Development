# Vehicle / map BSOD notes (lab)

## Root cause we fixed: wrong vehicle wired as NAL/TRIXX

With `--driver rtcore`, older `mapper_nal` still opened `\\.\Nal`, matched **TRIXX.sys** for the code cave, and DKOM-unlinked **TRIXX** — corrupt list or wrong `DriverEntry` target → **BSOD** or instant abort.

Current mapper uses `OpenVehicleDeviceHandle()`, `VehicleSysNameMatch()`, and `DkomUnlinkActiveVehicle()` per `--driver` flag.

## Safe defaults for RwTest

| Mode | Risk |
|---|---|
| `RwTest.sys` + `--rw-only` | No AC IOCTL sweep; **DKOM unlink off** (use `--dkom-unlink` only if you accept PG risk) |
| `Betest.sys` + `--full` / `--ac-preempt` | PatchGuard / delayed bugcheck risk |
| Stop BYOVD service after successful map | **Use-after-free** on hijacked `DRIVER_OBJECT` → BSOD |

After map: **do not** `sc stop` the vehicle; reboot to clear.

## Which vehicle when

| Vehicle | Typical failure |
|---|---|
| **NAL / iqvw** | Blocked on AC hosts; smoke-test fails if stale handle |
| **RTCore64** | Needs matching `--driver rtcore` (fixed in tree); service `RtCoreSvc` |
| **TRIXX** | Mapper restarts service for I-cache; wrong if you meant another `--driver` |
| **Corsair** | Different primitive; full-page path |

Check `%TEMP%\pf_diag.tmp` (write-through). Last lines before bugcheck show the stage.

## If you still BSOD

1. Reboot (clean vehicle state).
2. Map only `RwTest.sys` with `map-secure-rwtest.ps1` (no `--full`).
3. If log stops before `DriverEntry complete`, primitive/CR3/cave — verify vehicle device opens.
4. If BSOD seconds after success, avoid `--dkom-unlink` and any `--dbg` / DKOM flags.
