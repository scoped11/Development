# How to read / write (after map)

The mapper’s job is to get **Betest** into kernel and attach IRP handlers to the BYOVD `DRIVER_OBJECT`. After that, **usermode never uses NAL IOCTLs** for game memory. You open Betest’s device and send buffered IOCTLs.

## 1. Map once (stable)

```text
cd Development\byovd
.\build.ps1
copy vehicles\NalDrvIntel.sys bin\     :: or your vehicle next to mapper_nal.exe
run-map-stable.cmd
```

Expect `%TEMP%\pf_diag.tmp` to mention **RW-only** and **not stopping BYOVD**. Reboot between failed maps if Nal is in a bad state.

Mapper flags that still **leave R/W intact**:

| Flag | Effect |
|---|---|
| (default) | Map + device; no automatic AC preempt |
| `--driver nal\|trixx\|rtcore\|corsair\|ts\|siv` | Which vehicle |
| `--no-cleanup` | Extra skip of service cleanup (already skipped after successful map) |
| `--ac-preempt` / `--full` | **Also** fire existing bypass IOCTLs (BSOD/PG risk) |

## 2. Open the device

Betest creates a **dynamic** `\DosDevices\<prefix><digit>` name, falling back to a static pool.

Scan order (same as mapper):

- Dynamic: `\\.\MsFlt0` … prefixes in `include/betest_ioctl.h` × digits `0–9`
- Static: `\\.\MsFlt0`, `WdFilter1`, `FltMgr2`, `NdProxy0`, `TdiFilter`, `MsSecFlt`, `WfpDrv0`, `NtFilter1`

Helper: `BetestOpenDevice()` in `include/betest_ioctl.h`.

```c
HANDLE h = CreateFileA("\\\\.\\MsFlt0", GENERIC_READ|GENERIC_WRITE,
    FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
```

## 3. Session handshake (required)

Every IOCTL except auth is **`STATUS_ACCESS_DENIED`** until:

```c
ULONG64 challenge = GetTickCount64(), token = 0;
DWORD n = 0;
DeviceIoControl(h, IOCTL_SESSION_AUTH,  // CTL_CODE(..., 0x870, ...)
    &challenge, 8, &token, 8, &n, NULL);
```

`BetestSessionAuth(h)` does this.

## 4. Process memory (what overlays should use)

**Read** — `IOCTL_READ_PROCESS_MEM` (`0x805`)

Input (`pack(1)`):

| Offset | Type | Field |
|---|---|---|
| 0 | `uint64` | ProcessId |
| 8 | `uint64` | Address (target VA) |
| 16 | `uint32` | Size |
| 20 | `uint32` | Pad |

Output buffer: **raw bytes** of length `Size` (driver overwrites the system buffer).

```cpp
PROC_READ_REQ req{ pid, address, size, 0 };
DeviceIoControl(h, IOCTL_READ_PROCESS_MEM, &req, sizeof(req), out, size, &ret, nullptr);
```

Kernel path: `PsLookupProcessByProcessId` then copy (`MmCopyVirtualMemory` if resolved, else attach + `RtlCopyMemory` in `__try`). Invalid pages → `STATUS_ACCESS_VIOLATION`, not a guaranteed BSOD.

**Write** — `IOCTL_WRITE_PROCESS_MEM` (`0x806`)

Header same as read, then `Data[Size]` immediately after pad. Input length = `24 + Size`.

## 5. Kernel memory (mapper / diagnostics, not game objects)

| IOCTL | Function | In | Out |
|---|---|---|---|
| `IOCTL_READ_KERNEL_MEM` `0x802` | `KREQ_READ` `{Address, Size}` | kernel VA | bytes via `MmCopyMemory` |
| `IOCTL_WRITE_KERNEL_MEM` `0x803` | `KREQ_WRITE` `{Address, Size, Data[]}` | kernel VA | none |
| `IOCTL_ALLOC_KERNEL_MEM` `0x804` | `{Size, PoolType, Tag}` | — | `uint64` pool VA |

Kernel write uses a raw copy — **easy to BSOD**. Prefer process IOCTLs for user VA.

## 6. Example

`examples/process_rw.cpp` — reads a local `uint64` marker through Betest.

```text
cl /EHsc /std:c++17 examples\process_rw.cpp /I include /Fe:bin\process_rw.exe
bin\process_rw.exe
```

## 7. What this is not

- Not `ReadProcessMemory` / `NtReadVirtualMemory` (those hit Ob callbacks).
- Not DMA (separate `Development\dma` lane).
- Opening the **NAL** device after map often fails: Betest replaced `IRP_MJ_DEVICE_CONTROL` on that `DRIVER_OBJECT`.
