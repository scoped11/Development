# RwTest.sys — minimal driver for lab testing

**RwTest** is a small kernel driver (~few KB of logic) with only:

- Random blend name `\\.\MsFlt<N>` … (same scan list as Betest)
- `IOCTL_SESSION_AUTH` (`0x870`) — challenge + **PID-bound** session
- `IOCTL_DELETE_SYMLINK` (`0x808`) — optional hide after your client has a handle
- `IOCTL_READ_PROCESS_MEM` / `IOCTL_WRITE_PROCESS_MEM` (`0x805` / `0x806`)

No anti-cheat IOCTLs. Same contract as `include/betest_ioctl.h`.

## Build

```powershell
.\build.ps1
```

Outputs: `bin\RwTest.sys`, `bin\process_rw.exe`, `bin\mapper_nal.exe`.

## Path A — BYOVD map (matches your current lab)

Smaller payload → often **easier to debug** than full `Betest.sys`.

```text
copy vehicles\RTCore64.sys bin\
powershell -File map-secure-rwtest.ps1
bin\process_rw.exe
```

`map-secure-rwtest.ps1` / `run-test-rwtest.cmd` map with `--ac-safe`: same as quiet profile (hygiene + `--rw-only`). If **Rust/EAC blocks BYOVD on the game PC**, do **not** map there — use **Lane D** ([12_lane_d_split_trust.md](12_lane_d_split_trust.md)): DMA on game PC, BYOVD on no-AC VM, sync `build_id` + offsets.

Expect: `read 0x1122334455667788`.

## Path B — Service load (not recommended)

Service load leaves a **PiDDB / services** footprint and often needs **test-signing** (watermark). Prefer **Path A** only.

`install-test-driver.ps1` remains for isolated VM experiments; do not use on a host where you care about `testsigning` visibility.

## Path C — Full Betest payload

Same as before:

```text
run-map-stable.cmd
```

Device name may be dynamic (`MsFlt0`, etc.); `process_rw` scans the list including `RwTest0` first when mapped from RwTest only.

## What to use when

| Goal | Use |
|---|---|
| Test R/W IOCTL path only | **RwTest** + `run-test-rwtest.cmd` |
| AC bypass IOCTL research | **Betest.sys** + flags from doc 05 |
| Stable daily overlay dev | **RwTest** until RW proven, then Betest if you need extra IOCTLs |
