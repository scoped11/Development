# driver2 — full kernel driver capability (cheat-stack shape)

After **BYOVD map**, Driver2.sys is a **normal `DRIVER_OBJECT` dispatch path**: usermode talks to it with **`DeviceIoControl`** like commercial cheats do. No `ReadProcessMemory` required for game memory.

## Direct access

| Tool | Role |
|------|------|
| **`Driver2Client.exe`** | Direct IOCTL: probe, **kernel** `kread`, process `pread` / `pwrite` |
| **`sdk/driver2_api.h`** | Drop into your overlay / ESP project |
| **`include/betest_ioctl.h`** | Full IOCTL catalog (0x800–0x86A+ in payload) |

Open path: dynamic `\\.\MsFltN` / filter-style names (`BetestOpenDevice`). Session **`0x870`** required.

## What a “normal” mapped cheat driver does → driver2

| Capability | IOCTL / mechanism | driver2 |
|------------|-------------------|---------|
| Read/write **any process** | `0x805` / `0x806` | **Yes** — primary API |
| Read/write **kernel VA** | `0x802` / `0x803` | **Yes** — `Driver2Client kread` |
| Pool alloc in kernel | `0x804` | **Yes** (payload) |
| Strip callbacks / NMI / AC IOCTLs | `0x81B`, `0x868`, `0x869`, … | **Yes** — `--driver2` map sweep |
| Hygiene (PiDDB / CI / WdFilter) | Mapper pre-`DriverEntry` | **Yes** — `--driver2` |
| Works with **any title** | `--ac-replica universal` | **Yes** — EAC+BE+ACE masks together |
| Service `sc start` load | — | **No** — BYOVD map only (ranked live path = hijack + DMA per Lane D) |
| Module stomp (image-backed RIP) | `--stomp` / `Driver2Stomp.sys` | **Lab** — see `docs/STOMP_BYPASS.md` |

## Works with anything

- **Vehicle:** rtcore → corsair → nal fallback (mapper)
- **AC profile:** `rust` | `r6` | `deltaforce` | `arenabreakout` | `arksa` | **`universal`** (all improve masks)
- **Comm:** any usermode tool that includes `driver2_api.h`
- **Game:** any process **PID + VA** via `pread`/`pwrite` — not tied to one exe name

## Map first (your approval)

```powershell
.\driver2\everything.ps1 -Profile universal -Target vm -Execute
.\driver2\bin\Driver2Client.exe probe
.\driver2\bin\Driver2Client.exe pread 1234 0x7ff700000000 16
```

VM live map still waits on **`vm-test.ps1 -Execute`** when you say go.
