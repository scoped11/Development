# Single PC — driver + your EXE + play (no VM, no FPGA)

**What you asked for:** one machine, map driver, run cheat, play.

```powershell
# Elevated PowerShell
cd byovd\driver3
.\single-pc-play.ps1 -Game rust -Execute -Live
```

Or: `.\bin\Driver3Play.exe -Game rust -Live`

## What it does

1. Sync `build_id` from dumps (Rust / R6 / …)  
2. **Map Driver2 on THIS PC** (`driver2\ranked-play.ps1 -ForceMap -Execute`)  
3. Start **your cheat EXE** (default: `driver2\bin\cheat_dev_minimal.exe` if built)  
4. You launch the game — reads via **Driver2Session** / IOCTL (not DMA)

**No VM. No FPGA. One flow.**

## Rust + Rainbow Six

```powershell
.\single-pc-play.ps1 -Game rust -Execute -Live
.\single-pc-play.ps1 -Game r6 -Execute -Live
```

`-Live` sets ranked override (kernel on session with AC — **your risk**, not undetected).

## S grade (`single_pc_kernel`)

Check: `.\single-pc-s-tier.ps1` → `single_pc_s.json`

Needs: admin map success (`host_local_map.log` exit=0), Driver2 bins, build_id rust+r6.

## Honest limits

| Claim | Truth |
|-------|--------|
| One PC, exe, play | **Yes** |
| No extra hardware | **Yes** |
| Safe on live EAC/BE | **No** — Tier 0 titles + kernel on same box |
| Undetected | **Not claimed** |

Switch back to split-trust: `set-live-read-method.ps1 -Method dma` or `kernel_lab_s`.
