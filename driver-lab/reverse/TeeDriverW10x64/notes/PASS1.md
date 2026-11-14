# Pass 1 — TeeDriverW10x64.sys (Intel ME / HECI)

**SHA256:** `A2E0B9EA4973BC875BAF685C684ED007407D05AB0BB053C3FB72D6357106A571`  
**IDA session:** `174cb043` (clone under `bin/`)  
**LOLDrivers:** no  

## Architecture

| Item | Detail |
|------|--------|
| `DriverEntry` | `0x14003AB40` → KMDF bind (`WdfVersionBind` imports) |
| Model | **WDF** framework — IOCTL/file I/O via WDF queues, not naked `IRP_MJ_DEVICE_CONTROL` switch |
| Core HAL | `mei_hal.c` — `HalHeciReset`, HECI host reset state machine |
| Large handler | `sub_14001E984` (~2.6k) — ME reset / idle / PoFx paths, not user phys R/W |
| PnP resources | `sub_1400456D8` — parses **firmware resource descriptors**; **one** `MmMapIoSpace` for ME MMIO BAR |

## `MmMapIoSpace` (`sub_1400456D8`)

- Called while walking ACPI/CM resource list (switch on descriptor type `1/2/3/...`).
- Maps **device-reported** physical ranges for the ME coprocessor — lengths/addresses from **PNP**, not IOCTL input buffers.
- No `ZwOpenSection(PhysicalMemory)`, no `MmCopyMemory`, no HAL PCI user IOCTLs.

## User access

- Strings reference `EvtDeviceFileCreate` / `FileClose` (WDF), host ID tracking.
- Typical usermode path is **`\\.\HECI#...`** (interface GUID instances) via standard Intel MEI stack — not a single wide-open `\\.\Foo` like RTCore.
- `ZwOpenProcessTokenEx` import — ACL/token checks on privileged paths.

## Verdict (pass 1)

**Not a BYOVD mapper vehicle.**

- Attack surface = **Intel ME protocol / reset / power** — high complexity, no obvious arbitrary kernel R/W primitive.
- `MmMapIoSpace` is scoped to ME hardware bring-up.
- Running on host as part of MEI — fuzzing risks bricking ME features, not a clean R/W ladder.

## Queue

Next clone: `iaStorVD/`
