# Kernel ready — Rust + Rainbow Six (no FPGA)

**Grade `KERNEL_READY`:** map Driver2 in VM, read with your cheat or `Driver2Client`.  
**Grade `S_KERNEL`:** same + `host-b exit=0` + `kernel_proof` in manifest (full Scale S for `kernel_lab_s`).

**Not:** live ranked on **this PC** with EAC/BE and no hardware. That is **`dma`** mode.

## One-time host

```powershell
cd byovd\driver3
.\set-live-read-method.ps1 -Method kernel_lab_s
.\kernel-ready.ps1 -MapVm
```

## VM (elevated) — unlock S_KERNEL

```powershell
Z:\byovd\driver3\vm-enable-lab-uac-off.ps1
# reboot guest
Z:\byovd\driver3\register-driver3-elevated-task.ps1
Z:\byovd\driver3\host-b.ps1 -Profile rust -Execute -ForceMap -SkipProve
Z:\byovd\driver3\host-b.ps1 -Profile r6 -Execute -ForceMap -SkipProve
```

## Run (VM, after map)

**Rust (EAC profile at map time):**

```text
Z:\byovd\bin\Driver2Runner.exe --profile rust --mode bypass --force-map
Z:\byovd\bin\Driver2Client.exe pread <RustClient_pid> <va> 8
```

**Rainbow Six (BattlEye profile):**

```text
Z:\byovd\bin\Driver2Runner.exe --profile r6 --mode bypass --force-map
Z:\byovd\bin\Driver2Client.exe pread <RainbowSix_pid> <va> 8
```

C++: `driver2\sdk\Driver2Session.hpp` — see `examples\cheat_dev_minimal.cpp`.

## Status

```powershell
.\kernel-ready.ps1 -ReportOnly
```

Output: `kernel_ready.json`
