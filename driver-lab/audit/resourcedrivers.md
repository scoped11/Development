# Resource drivers (lab memory)

Updated 2026-11-13. Guest is `DriverLab-E-26200` (Windows 11 26200, MS blocklist on, 8192 MB RAM, VM state stayed `running` through every map attempt below). Payload target is `mapper_nal.exe` mapping `RwTest.sys` (`--no-auth --rw-only --no-fallback`), then `process_rw.exe`.

**Nothing has completed that full map.** Two vehicles have a proven physical read/write IOCTL. Both died later, in CR3 bootstrap, with no bugcheck.

Update this file when a load, open, IOCTL, IDA session, or map result changes. Do not treat a probe `write_rmw_ok` as a finished map.

## What “works” means

| Step | Meaning |
|------|---------|
| Load | `sc start` win32 0 on this VM |
| Open | `CreateFile` on the real device name |
| Phys | Arbitrary physical read and write from usermode (probe or IOCTL) |
| CR3 | `mapper_nal` printed a validated kernel CR3 |
| Map | `mapper_nal` exit 0 and `process_rw` talked to the mapped `RwTest` |

Known failure codes on this guest: **1275** blocklist, **577** bad signature, **31** driver returned failure (usually missing its vendor stack).

## Wired into `mapper_nal` (`--driver`)

| Flag | File | Device | IDA | Load | Open | Phys | CR3 | Map | Notes |
|------|------|--------|-----|------|------|------|-----|-----|-------|
| `myport` | `MyPortIO_x64.sys` | `\\.\MyPortIODev` | `0e7cd77a` | Yes | Yes | **Yes** | No | No | Best new OEM find. See below. |
| `corsair` | `CorsairLLAccess64.sys` | `\\.\CorsairLLAccess` | In `primitive_corsair.hpp` (not a saved IDB id in the audit notes) | Yes | Yes | **Yes** | No | No | Full-page map. Probe is the regression vehicle. |
| `msio` | `MsIo64.sys` | `\\.\MsIo` | `b71c23ac` | Yes | Yes | **No** | No | No | Map IOCTL gle 998. Chipset gate. |
| `ts` | `ThrottleStop.sys` | ThrottleStop device | Not recorded | Yes | Not proven this run | Weak | No | No | 8-byte IOCTL. Must start from `%TEMP%`, not `Z:\` (error 193). NonCached PTE reads failed on 26200. |
| `rtcore` | `RTCore64.sys` | `\\.\RTCore64` | Not this inbox | Not in this matrix | — | No on current builds | — | — | Afterburner after 4.6.2 maps only VGA ROM, not CVE-2019-16098 arbitrary phys. |
| `trixx` | `TRIXX.sys` | Sapphire TRIXX | Primitive exists | Not staged in this matrix | — | — | — | — | Default flag in `main_nal.cpp` if no `--driver`. |
| `siv` | `SIVX64.sys` | SIV | Primitive exists | Not staged | — | — | — | — | |
| `nal` | `NalDrvIntel.sys` / `iqvw64e.sys` | `\\.\Nal` | Primitive exists | Blocklist-class | — | — | — | — | Do not expect this on 26200. |

Code: `byovd/src/mapper/primitive_*.hpp`. Probe: `byovd/bin/VehiclePhysProbe.exe corsair|myport|msio|rtcore`.

### MyPortIO_x64 — phys works, map does not

- Service tag `MyPortIO`. Kernel device `\Device\MyPortIO0`.
- **Read** `0x9C406680`, exactly **12** bytes in/out (`u64 Phys`, `u32 Size=4`). `sizeof` 16 returns gle 87.
- **Write** `0x9C40A684`, **16** bytes (`Phys`, `Size`, `Value`). Decompile case is `-1673484668`, not `0x9C40A584`.
- `MmMapIoSpace` dword, no chipset check. `DriverEntry` sets the phys branch on (`*(a1+20)=1`).
- VM probe job `9dbeb51d0aaf`: `pa=0x1100 read=0x00257000 write_rmw_ok`.
- x86 twin `MyPortIO_x86.sys` is **1275**.
- Full map jobs (`d24819a719e6`, `5c0e195d1275`, `4857e1d57101`) never printed a CR3. Latest run (low RAM only, under 512MB) got as far as `MyPort EPROCESS scan 0x100000-0x20000000` and then produced no further stdout for 6 minutes. Host wait exit 2. VM did not crash.
- Why it stalls: each dword is its own IOCTL. A 16MB tick is thousands of calls. Do not scan PA `>= 0x100000000`. `MmMapIoSpace` of `0x100000000` wedged the mapper with no bugcheck.
- Detail: `driver-lab/audit/myportio_x64_ioctl_2026-11-13.md`.

### CorsairLLAccess64 — phys works, map does not

- IOCTLs: map `0x225374`, unmap `0x229378`, MSR read `0x225388`, MSR write `0x229384`.
- One IOCTL maps a full page into usermode. Probe: `write_rmw_ok` at `pa=0x1000`, MZ-style sig `0xd0e8`, several runs (`449834e4f458` and earlier).
- Matrix row showed open error 2 because it tried the wrong name. The working name is `\\.\CorsairLLAccess`.
- `mapper_nal --driver corsair` passed smoke map of `0x1000`, then stuck in `CorsairBootstrapCR3` Phase3. First builds scanned high RAM and hung on the first map of PA `0x100000000`. Later builds were told to stay below 4GB. Still no validated CR3 and no `process_rw`.
- On LOLDrivers. VM lab only.

### MsIo64 — loads, phys map does not

- IOCTL map `0x80102040`, unmap `0x80102044`, port `0x80102050` / `0x80102054`. 40-byte packet.
- VM: service running, `\\.\MsIo` opens. Phys map gle **998**. `MemortAccessBase` stays 0 because the guest has no matching Intel chipset DID.
- `MsIo32.sys` is **1275**.
- Do not retry `mapper_nal --driver msio` on this guest until the chipset gate is bypassed. IDA `b71c23ac`.

### ThrottleStop — loads, primitive is weak here

- `--driver ts`. Loads in the matrix (win32 0).
- Previous lab: page-table reads through its 8-byte path failed on 26200. Not re-proven as a full map in this session.
- Next try only from `C:\DriverLab\map` or `%TEMP%`, with a short stall kill. Expect it to fail the same PTE way.

## IDA done, not a mapper vehicle

| Driver | IDA | Device | Load | Why it is not our exploit |
|--------|-----|--------|------|----------------------------|
| `PawnIO.sys` 2.2.0 | `e8b865f4` | `\\.\PawnIO` | Yes | `MmMapIoSpace` only after RSA-signed pawn bytecode. Not a raw IOCTL. |
| `cam_driver_mb.sys` | `78d6f4b6` | `\\.\CAMDriverMB` | Yes | NZXT Super I/O / GPIO. `MmMapIoSpaceEx` is inside the Nuvoton/ITE probe, not a user phys R/W. Matrix open failed on the wrong name `HidFanControl`. |
| `ssdevfactory.sys` | `a375391f` | none (`\\.\` string absent) | Yes | SteelSeries USB factory. No `MmMapIoSpace`. |
| `AMDRyzenMasterDriver.sys` (+ `,1` `,2`) | `dd184ae0` | `\\.\AMDRyzenMasterDriverV22` | **31** | `MmMapIoSpace` helper `sub_140004354`, but start needs the Ryzen Master stack. Not a bare `sc start`. |
| `cpuz165.sys` | `1804a560` | `\\.\CPUZ165` | **1275** | Version above the blocklist max. Carved build is not loadable. |

**VM smoke (2026-11-13):** `SONiXDDRx64` + `iocbios2` load/open OK. `SIVX64` v5.88: **`RtlAdjustPrivilege(10)`** then **`\\.\Global\SIVDRIVER`** (job `88002f68eeb9`). Phys read is raw IOCTL **`0x10`** (16 bytes at PA `0x1000` returned `E8 D0 97 DE …`, job `476bde47e84c`). IOCTL `0x14` with a 16-byte buffer returns win32 **24**.

## Inbox loaded, no exploit wired

These returned win32 0 on 2026-11-13 (`vm_driver_matrix.csv`) and were not given a `--driver` flag. Matrix “open” is only meaningful when the device column is a real name. Error 2 means the guessed path was wrong or there is no DOS device.

| Driver | Open in matrix | IDA | Exploit status |
|--------|----------------|-----|----------------|
| `AsrNfDrv000.sys` | fail 2 | No | Unknown. Not a known phys map. |
| `CorsairGamingAudio64.sys` | no guess | No | Audio. Unlikely phys. |
| `CorsairVBusDriver.sys` | no guess | No | Virtual bus. |
| `CorsairVHidDriver.sys` | no guess | No | HID. |
| `hidkmdf.sys` | no guess | No | HID. |
| `ICCWDT.sys` | fail 2 | No | Intel watchdog. Not phys-map. |
| `msihid.sys` | no guess | No | HID. |
| `PawnIO_2.2.0_vm.sys` | no guess | Same family as PawnIO | Same bytecode gate. |
| `ScpVBus.sys` | no guess | No | Virtual gamepad bus. |
| `SONiXDDRx64.sys` | **OK** `\\.\SONiXDDR_0_0_0_1` (smoke) | Partial (`951aed9d`) | Phys IOCTL **`0x9C406684`** (low 1MB). LOL listed. `sonix_iocbios_ida_2026-11-13.md`. |
| `ssbthid.sys`, `sshid.sys`, `ssps2.sys`, `sssmbus.sys` | no guess | No | SteelSeries HID/SMBus. Low priority. |
| `STTub30.sys` | no guess | No | ST USB. |
| `VirtualHID.sys` | no guess | No | HID. |

## Did not load

| Driver | Code | IDA | Notes |
|--------|------|-----|-------|
| `cpuz165.sys` | 1275 | `1804a560` | Blocklist version rule. |
| `MsIo32.sys` | 1275 | No | Use `MsIo64` only, and that one still fails phys. |
| `MyPortIO_x86.sys` | 1275 | No | Use the x64 binary. |
| `SiLib.sys`, `SiUSBXp.sys` | 577 | No | Signature rejected. |
| `AMDRyzenMasterDriver*.sys` | 31 | `dd184ae0` | Needs the full vendor stack. |

## Map attempts (this session)

| Job | Vehicle | Result |
|-----|---------|--------|
| `449834e4f458` and earlier probes | Corsair | Phys `write_rmw_ok`. Not a full map. |
| `9dbeb51d0aaf` | MyPort | Phys `write_rmw_ok` at `pa=0x1100`. |
| `f4d8be7f5fc1` | MsIo | IOCTL map gle 998. |
| `031f9d7ac219`, `8a356370d8c7`, `b0303c73f294` | Corsair `mapper_nal` | Host timeout. Stuck in CR3. Killed. No bugcheck. |
| `d24819a719e6`, `5c0e195d1275`, `4857e1d57101` | MyPort `mapper_nal` | Host timeout. Service started. No CR3. Killed. No bugcheck. |

Worker footgun, fixed in `driver-lab/vm/bg/worker.ps1`: an exec job that named a `.ps1` used to run `-File` and **drop** `-DriverFlag`, so MyPort requests silently ran as Corsair. Current worker uses `-Command` with the full string. Confirm the guest copy is `C:\DriverLab\bg\worker.ps1` after `guest-ensure-worker.ps1`.

## Do not repeat

- Do not scan or `MmMapIoSpace` physical address `0x100000000` and above on this guest. That call wedged `mapper_nal` and the VM stayed up.
- Do not walk all of the 8GB with MyPort’s 4-byte IOCTL. Six minutes produced zero progress past the scan banner.
- Do not use MsIo for the map on this VM.
- Do not `sc start` CPU-Z, MyPort x86, or MsIo32 here. They are 1275.
- Do not treat PawnIO, CAM, or ssdevfactory as raw phys vehicles. IDA already closed those.
- Killing `mapper_nal` from a non-elevated Guest Additions session often misses it. Use the elevated worker or `guest-kill-stuck-mapper.ps1`.

## Online / greenfield research

See **[compromising_drivers_research_2026-11-13.md](compromising_drivers_research_2026-11-13.md)** — Tier A candidates (GDrv variant, SIVX64, AmdTools64, AppShopDrv103, IOMap64, pstrip64, DriversCloud), chain drivers (Redirt/LallaMon), and download map. **Research only;** hash + IDA before adding `--driver`.

## Next methods (in order)

1. **Corsair, low RAM only, one map per page, first 32MB.** Phys IOCTL is already proven and a page is one IOCTL, so CR3/EPROCESS can be searched in usermode after the map. Stay below 4GB.
2. **MyPort EPROCESS spot check**, not a linear 512MB walk: one dword at `ImageFileName` `0x338` and `0x5A8`, PID `0x1D0` / `0x440`, `DirectoryTableBase` `0x28`. Print every 1MB. Kill at 3 minutes with no new line.
3. **ThrottleStop from `C:\DriverLab\map`** only if the two above still have no CR3. Expect the old NonCached PTE failure.
4. **IDA then VM** for `SONiXDDRx64.sys` and finish `iocbios2.sys` (`6203e2af`) before any `mapper_nal` flag. `cam_driver_mb` is already a no.
5. New OEM only after IDA shows a weak phys or full-page IOCTL and a device name. Then add a row to this file before the map run.
