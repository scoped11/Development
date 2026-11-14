# Kernel Lab route (no FPGA) — `kernel_lab_s`

| | |
|--|--|
| **Method** | Driver2 on **VM only** |
| **Hardware** | None (no DMA card) |
| **Reads** | `Driver2Client pread` / `Driver2Session::ReadProcess` after map |
| **Hooks** | Your cheat process / lab targets — not AC live host default |
| **1 PC** | Yes (VM on same box) |

```powershell
set-live-read-method.ps1 -Method kernel_lab_s
kernel-lab.ps1 -Profile rust -Execute   # plan / host orchestration
host-b.ps1 -Profile rust -Execute -ForceMap   # in elevated VM
```

**Not for:** Tier 0 ranked on `DESKTOP` with EAC/BE/ACE without DMA.
