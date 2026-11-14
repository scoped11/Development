# Map flow vs kdmapper diagram

Your diagram is the **canonical BYOVD mental model**. This kit follows the same phases; only the **vehicle IOCTL shape** and **DriverEntry trigger** differ from stock kdmapper.

## Side-by-side

| Stage | kdmapper (diagram) | `mapper_nal` (this kit) |
|---|---|---|
| User loader | kdmapper.exe | `mapper_nal.exe` |
| Payload bytes | unsigned `.sys` on disk | `RwTest.sys` / `Betest.sys` |
| Into kernel | IOCTL + vehicle R/W | BYOVD primitive: virtual kernel R/W (NAL IOCTL, RTCore phys walk, Corsair full-page, …) |
| Vehicle | `iqvw64e.sys` → `\\.\Nal` IOCTL **0x80862007** (Intel disclosure) | `--driver rtcore` → `RTCore64.sys` IOCTL **0x80002048** / **0x8000204C**; or `nal` for Intel path |
| Allocation | Pool / independent pages via iqvw helpers | `ManualMap()` → `AllocateKernelMemory` (vehicle-specific) |
| `DriverEntry` | `CallKernelFunction(entry, …)` | HAL dispatch trampoline + `NtQueryIntervalProfile` (no fake `DRIVER_OBJECT`) |
| User → driver I/O | Varies (often another device) | `CreateFile` blend name + `IOCTL_SESSION_AUTH` + `0x805`/`0x806` R/W |
| Post-map scrub | PiDDB, hash bucket, MmUnloaded, … | Partial (DKOM optional; RwTest trace IOCTLs in progress); see `reference/TRACE_SURFACES.md` |

So **yes** — you should be doing *this* pattern: **signed vehicle → map unsigned payload → run `DriverEntry` → talk to payload device**. You should **not** expect byte-for-byte kdmapper IOCTLs when using **RTCore** or other vehicles.

## Lab command (RTCore vehicle you have)

```text
map-secure-rwtest.ps1
```

Equivalent:

```text
bin\mapper_nal.exe bin\RwTest.sys --driver rtcore --no-auth --rw-only --stealth 3f
```

## Anti-hook (usermode)

Before map, the mapper can scan exports used for **`DeviceIoControl` / `NtDeviceIoControlFile` / `NtQueryIntervalProfile`** for inline jumps (common AC / monitor hooks). Enabled by default for `RwTest.sys`; disable with `--no-anti-hook`.

Kernel-side callback / AC “anti-hook” IOCTLs live in **Betest** (`0x81C`, `0x83E`, …) and stay **off** under `--rw-only` to avoid BSOD risk.
