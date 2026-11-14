# Pass 1 — rtwlane613.sys (clone `bin/rtwlane613.sys`)

**SHA256:** `30925A8CE11536888CE4FAAFF4C55A831FE227B58BDA2780F62B8D04ADFC2850`  
**IDA session:** `170c9bb1`  
**LOLDrivers:** no  

## Surface map

| Item | Detail |
|------|--------|
| `DriverEntry` | `0x140040780` → WDF/NDIS init |
| Device create | `sub_1404F0CC4` — name = `\Device\Realtek_Wifi_Device` + **instance GUID** (same for DosDevices) |
| SDDL | `D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;AU)(A;;GA;;;AC)` — very permissive |
| `IRP_MJ_DEVICE_CONTROL` | `sub_1404F22C0` → `sub_1404EF378` |
| `MmMapIoSpaceEx` | **1** direct call site — `sub_14002A468` via **WifiCx / resource parser** (`WifiCxGetResources` string), not user buffer |

## IOCTL gate (`sub_1404EF378`)

Only handles a **small fixed set** of control codes (decimal):

| Code (dec) | Hex (full) | Behavior |
|------------|------------|----------|
| 1188884 | 0x001221B4 | `sub_1404EFE10` (type 0) |
| 1188888 | 0x001221B8 | `sub_1404EFE10` (type 1) |
| 1188892 | 0x001221BC | counter only → default `STATUS_NOT_SUPPORTED` path |
| 1188896 | 0x001221C0 | counter only |
| 1188900 | 0x001221C4 | `sub_1404EFE10` (type 4) |
| 1188904 | 0x001221C8 | `sub_1404EFF8C` + completion helper (uses IRP buffer + length) |

Unknown codes increment stats (`Unknown ioctl` debug string in `sub_1400CFB64`).

Private NDIS/WifiCx dispatch tables (`qword_140EF9078` vtable) handle most traffic — not a flat RTCore-style switch.

## Verdict (pass 1)

**Not a BYOVD mapper vehicle.**

- Phys map is for **firmware/WifiCx memory resources**, not arbitrary physical addresses from IOCTL input.
- User-visible device name is **GUID-suffixed** — opening `\\.\Realtek_Wifi_Device` may not match loaded instance without enumerating the full symlink.
- 15 MB NDIS miniport + mp_* / hal_* — attack surface is WiFi OIDs, not kernel R/W primitives.
- Running on production host; AC monitors network stack.

## Next clone (queue)

When pass 1 signed off → `driver-lab/reverse/TeeDriverW10x64/` (then `iaStorVD/`).
