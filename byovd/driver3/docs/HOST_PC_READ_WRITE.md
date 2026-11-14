# This PC (Host A) — what you are doing and how to read/write

You are on the **game / manifest machine** (`$env:COMPUTERNAME`). Live anti-cheat from your dumps (EAC, BE, ACE) puts this session in **Tier 0**: **no Driver2 map here** while AC is on the same Windows instance.

## What this PC is for

| Job | Tool | Output |
|-----|------|--------|
| Decide Host A vs B | `lane-gate.ps1 -Game <title>` | Tier 0 → stay on DMA path |
| Sync dump → build id | `patch-build-ids.ps1` | `docs/lab_manifest.json` |
| Ranked session prep | `ranked.ps1 -Game rust -PreflightOnly -SyncBuildId` | `ranked_session.json` |
| **Live memory reads** | `dma/` (`RobRunDMA.exe`, **FPGA**) | Overlay R/W in game |
| Orchestration | `reach-scale-s.ps1`, `developer-nod.ps1` | `dashboard.json`, `s_tier.json` |
| Drive the VM | `vm-test-from-host.ps1`, `vm-guest-hostb-from-host.ps1` | `vm_hostb_execute.log` on `Z:\` |

**Kernel Driver2 IOCTL proof runs only in the VM** — see [VM_GUEST_READ_WRITE.md](VM_GUEST_READ_WRITE.md).

## How to READ on this PC (ranked / live)

```text
Game process (RustClient, etc.)
  → physical RAM
  → FPGA DMA card (LeechCore fpga://)
  → VMMDLL in RobRunDMA.exe
  → offsets.h from dumps/<date>/
```

```powershell
cd C:\Users\baile\OneDrive\Desktop\Development
.\dma\scripts\build-windows.ps1
# FPGA connected → run:
.\debug\RobRunDMA.exe
```

Manifest must match the game build:

```powershell
cd byovd\driver3
.\ranked.ps1 -Game rust -Execute -SyncBuildId
```

Reads use **guest virtual addresses** resolved in the DMA engine — not `Driver2Client pread` on this host during ranked play.

## How to WRITE on this PC (ranked)

Same DMA path as reads (if enabled in your `dma/` build). **Higher risk** on live AC — Lane D still allows it technically via overlay, not via mapped kernel driver on this session.

Do **not** use for ranked on Tier 0:

```text
mapper_nal + Driver2.sys  →  blocked / ranked fail-closed
```

## How to sync with the VM

Shared folder: **`Z:\byovd`** inside guest = this repo’s `byovd\` tree.

| Host action | VM sees |
|-------------|---------|
| Edit `docs/lab_manifest.json` | Same path under `Z:\byovd\docs\` |
| `sync-build-id-from-dumps.ps1` | VM Host B uses same `build_id` |
| `vm-guest-hostb-from-host.ps1` | Writes `vm_hostb_execute.log`, `kernel_proof_*.json` on share |

After VM map:

```powershell
.\sync-guest-artifacts.ps1 -AllKernelProofs
.\sync-kernel-proof-from-artifacts.ps1
```

## One-time host setup

```powershell
.\bin\Driver3Setup.exe              # menu + cheat-dev guide
.\bin\Driver3Setup.exe --auto       # full Host A pipeline
.\setup-host-driver3.ps1            # PowerShell equivalent
```

## Scoreboards on this PC

```powershell
.\build-dashboard.ps1 -AllTitles          # Scale A
.\build-s-tier.ps1 -AllTitles             # Scale S
```

Target: **100% Scale A** and **grade S** after VM Host B proof + dynamic inspector (see `SCALE_S.md`).
