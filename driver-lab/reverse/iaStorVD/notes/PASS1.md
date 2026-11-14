# Pass 1 — iaStorVD.sys (Intel VMD RST)

**SHA256:** `69F5CE7E7E36ADEB26BA50F10483B69008E128DCE0F79D79B50B03C0498CF20B`  
**IDA session:** `e029db2b`  
**LOLDrivers:** no  
**Runtime:** **Running**, Boot start — Intel Chipset VMD RST Controller

## Architecture

| Item | Detail |
|------|--------|
| Type | **StorPort** storage miniport (imports `StorPortInitialize`, `StorPortGetDeviceBase`, …) |
| `DriverEntry` | `0x1403DD000` → `sub_140016BF0` (miniport registration) |
| User device | **None** — no `\\DosDevices\\` strings; IOCTLs go through **storport.sys** / class drivers, not a public `\\.\` BYOVD handle |
| Domain | NVMe/RAID/VMD (`RaidIsm`, `IntelNvm`, `NvmePort::processSetTemperatureThresholdIoctl` = storage IOCTLs internal to driver) |

## `MmMapIoSpace` usage

Six call sites (e.g. `sub_1400D7600`, `sub_1400D7830`, `sub_1400D8460`).

Example `sub_1400D7600`:

- Maps **fixed** `PHYSICAL_ADDRESS 0xC0000`, length `0x30000` (legacy PCI config/MMIO probe window).
- Walks structures scanning for Intel vendor **`0x8086`**, PCI header signature `0x52494350` (`"PCIR"`).
- **No** user buffer fields — enumeration for VMD/NVMe attach.

`MmGetPhysicalAddress` / `StorPortGetPhysicalAddress` used for **DMA/SGL** (normal storage path).

## Other notable imports

- `PsSetCreateProcessNotifyRoutineEx` — process notify (telemetry/hardening surface, not a phys primitive).
- `ExGetFirmwareEnvironmentVariable` — UEFI var reads (RST config).
- `IoBuildDeviceIoControlRequest` — **sends** IOCTLs to lower drivers, not a usermode dispatch table.

## Verdict (pass 1)

**Not a BYOVD mapper vehicle.**

- Boot-critical storage stack; no RTCore-style usermode device.
- Phys mapping is **hard-coded probe windows** + StorPort BAR helpers — not arbitrary IOCTL phys R/W.
- Research value = storage bugs only (high risk to OS disk); unrelated to kernel map ladder.

## Greenfield queue (this host)

| # | Driver | Verdict |
|---|--------|---------|
| 1 | rtwlane613 | Not vehicle |
| 2 | TeeDriverW10x64 | Not vehicle |
| 3 | iaStorVD | Not vehicle |

Next research target on this PC: **ACE-GAME.sys** (not in queue doc — separate consent/scope).
