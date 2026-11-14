# Legit OEM drivers — IDA + VM (26200, blocklist on)

**VM:** DriverLab-E-26200, elevated worker (`host-bg-admin.ps1 -Action exec`).  
**Probe:** `driver-lab/vm/bg/guest-legit-driver-probe.ps1` + `byovd/bin/VehiclePhysProbe.exe corsair|msio`.

### MsIo64.sys (IDA `b71c23ac`, loads on 26200 VM)

| IOCTL | Code | Buffer |
|-------|------|--------|
| `IOCTL_MSIO_MAPPHYSTOLIN` | `0x80102040` | 40-byte `Size, Phys, Section, Linear, Object` |
| `IOCTL_MSIO_UNMAPPHYS` | `0x80102044` | same layout for unmap |
| Port in/out | `0x80102050` / `0x80102054` | legacy I/O |

Device `\\.\MsIo`, service tag `MsIo`. Mapper: `--driver msio` via `primitive_msio.hpp` + `OpenMsIo`. VM matrix: **loads + device opens**; **phys map IOCTL often fails** when `MemortAccessBase` stays 0 (no matching Intel chipset DID in guest).

### MyPortIO_x64.sys — works on this VM (IDA `0e7cd77a`)

Loaded with win32 0 (not 1275). Device `\\.\MyPortIODev` (not `\\.\MyPortIO`). `MmMapIoSpace` dword IOCTLs `0x9C406680` (read, 12-byte in) and `0x9C40A684` (write, 16-byte in). Elevated probe `9dbeb51d0aaf`: `pa=0x1100 read=0x00257000 write_rmw_ok`. Mapper: `--driver myport`.

## VM matrix (2026-11-13)

| Driver | Load (`sc start`) | User open (`CreateFile`) | Phys R/W test |
|--------|-------------------|--------------------------|---------------|
| **CorsairLLAccess64** | (probe loads via service) | OK | **write_rmw_ok** at PA 0x1000 (repeated runs) |
| **cpuz165** (carved) | **1275** CI blocklist | N/A (not loaded) | Not run |
| **AMDRyzenMasterDriver** | **31** (device not functioning) | N/A | Not run |
| **PawnIO** 2.2.0 | **RUNNING** (leftover svc; `sc create` 1072) | `\\.\PawnIO` err **2** | Not a map driver |
| **cam_driver_mb** | **RUNNING** | `\\.\HidFanControl` err **5** | Not run |

### Corsair (regression vehicle)

```
[+] mapped pa=0x1000 va=... sig=0xd0e8 write_rmw_ok mark=0x....
```

Confirms IOCTL map + **read-modify-write-restore** on mapped VA in guest with blocklist policy enabled.

### CPU-Z

- Symlink (IDA strings): `\DosDevices\CPUZ165` → open as `\\.\CPUZ165`.
- Carved `cpuz165.sys` fails load with **1275** — matches MS **version** rule for `cpuz.sys` (policy max **1.0.4.9**); carved PE reports **1.0.6.5** class versions.
- **Next:** test inbox `cpuz141.sys` / signed build ≤ max version if we extract from official CPU-Z installer.

### AMD Ryzen Master (ASRock bundle)

- Symlink: `\DosDevices\AMDRyzenMasterDriverV22`.
- IDA: `sub_140004354` → **MmMapIoSpace** read helper; dispatch `sub_140004754` uses **HalGet/SetBusDataByOffset**-style path (`sub_140004C44` / `sub_140002F44`), not a trivial `DeviceIoControl` IOCTL table.
- `sc start` **31** on bare `sc create` — likely expects full stack (ACPI/IOCTL companion from Ryzen Master / board utility), not manual kernel service load.
- **Next:** IOCTL discovery from IRP_MJ_DEVICE_CONTROL handler + optional official installer on VM.

### PawnIO 2.2.0 (inbox + VM lab load)

| Check | Result |
|--------|--------|
| **26200 load** | **RUNNING** from `C:\DriverLab\legit\PawnIO.sys` (manual/lab `sc`; service START_TYPE disabled but running) |
| **MS blocklist / LOLDrivers** | Clear (SHA256 `fca6e7d58b0cf38d…`) |
| **Setup extract (host)** | `PawnIO_setup_2.2.0.exe` → inner zstd `105~` holds signed `PawnIO.sys` + `PawnIO.inf` + `pawnio.cat` → staged `oem-sys\pawnio-2.2.0-package\` |
| **VM install path** | Use **`guest-driver-package-install.ps1`** (`pnputil /add-driver`); **do not** run GUI setup (hangs worker). VBox `host-guest-exec.ps1` for admin when worker stuck. |
| **IDA** `e8b865f4` | `\DosDevices\PawnIO`, `MmMapIoSpace`, `HalGet/SetBusDataByOffset`, **BCryptVerifySignature**, VM opcodes `call_native` / `get_native` |
| **BYOVD / raw phys write** | **Not compatible** as mapper vehicle — phys/MMIO only through **RSA-signed pawn bytecode**, not arbitrary IOCTL map/write |

### NZXT cam_driver_mb

- Loads; guessed `HidFanControl` gives **access denied** (5) — wrong name or ACL (needs CAM service).
- **Next:** string xref / setup package for real device name.

## Tooling added

- `guest-legit-driver-probe.ps1` — load + open smoke + Corsair phys write via `VehiclePhysProbe`.
- `worker.ps1` / `host-bg-admin.ps1` — `legit_probe` action (re-run `install-worker.ps1` on guest to pick up).

## IDA sessions

| Driver | IDB | Notes |
|--------|-----|--------|
| AMDRyzenMasterDriver.sys | `dd184ae0` | MmMapIoSpace in 0x140004354 |
| cpuz165.sys | `1804a560` | Device `CPUZ165`; no Hex-Rays |
