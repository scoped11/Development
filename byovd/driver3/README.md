# driver3 — Lane D split-trust SDK

**Scale A target (80–90% SDK bar):** [`SCALE_A.md`](SCALE_A.md) · `build-dashboard.ps1`  
**Scale S (evidence shop grade):** [`SCALE_S.md`](SCALE_S.md) · `reach-scale-s.ps1` · `s_tier.json`

**This PC vs VM:** [docs/HOST_PC_READ_WRITE.md](docs/HOST_PC_READ_WRITE.md) · [docs/VM_GUEST_READ_WRITE.md](docs/VM_GUEST_READ_WRITE.md) · [docs/REACH_100_PERCENT.md](docs/REACH_100_PERCENT.md)

```powershell
.\setup-tool\build.ps1 -CopyToByovdBin
.\bin\Driver3Setup.exe --auto    # guided setup + cheat-dev explanation
.\setup-host-driver3.ps1          # PowerShell-only equivalent
.\complete-100-percent.ps1 -AllowStaticApproval
```

**driver2** = BYOVD kernel lab on **Host B** (no live AC).  
**driver3** = **split trust**: **Host A** (game PC) DMA + static offsets · **Host B** (VM) driver2 proof · shared **`build_id`** in `docs/lab_manifest.json`.

**No FPGA, no VM, one PC:** [`SCALE_S_SINGLE_PC.md`](SCALE_S_SINGLE_PC.md) · elevated `single-pc-play.ps1 -Game rust -Execute -Live` · `Driver3Play.exe` (**kernel on this PC** — not undetected on live AC).

**No FPGA, VM kernel lab:** [`SCALE_S_KERNEL_LAB.md`](SCALE_S_KERNEL_LAB.md) · `kernel_lab_s` / `kernel-ready.ps1`.

Doc: [`../docs/12_lane_d_split_trust.md`](../docs/12_lane_d_split_trust.md)

| Host | Tooling | Kernel map on session? |
|------|---------|-------------------------|
| **A** — game + EAC/BE/ACE | `dma/`, Il2Cpp / dumps | **No** (Tier 0 default) |
| **B** — lab VM | `driver2/` prove → ioctl → inspector | **Yes** (when you `-Execute`) |

**Not claimed:** D4 live ranked with BYOVD on the same box as AC. **D3i** stays on VM via `driver2\developer-approval-gate.ps1 -Inspector`.

---

## Quick start

Supported titles: **Rust (EAC)**, **R6 / ARKSA (BattlEye)**, **Delta Force / Arena Breakout (ACE)** — aliases in `games.json` (`df`, `abi`, `ark`, …).

```powershell
# Ranked (game PC — Host A)
.\driver3\ranked.ps1 -Profile rust -Execute -SyncBuildId

# One title
.\driver3\lane-gate.ps1 -Game rust
.\driver3\lane-gate.ps1 -Game df          # → deltaforce

# All titles → driver3_gates_all.json
.\driver3\lane-gate.ps1 -AllTitles
.\driver3\gate-all.ps1

# Full split workflow — plan only
.\driver3\everything.ps1 -Profile arenabreakout -WhichHost auto
.\driver3\everything.ps1 -AllTitles

# Host A only — DMA build + manifest hints (ranked reads path)
.\driver3\host-a.ps1 -Profile rust -Execute

# Host B only — delegate to driver2 VM bar
.\driver3\host-b.ps1 -Profile rust -Execute -ForceMap -Inspector

# Ranked default on game PC
.\driver3\ranked-play.ps1 -Profile rust
```

### VM test (DriverLab)

Share mount in guest is **`Z:\`** (not `D:\hostdev` on this box).

```powershell
# From host — plan smoke (works; proved scripts + gate on guest)
.\driver3\vm-test-from-host.ps1 -Profile rust

# Live map needs *Administrator inside guest* (Guest Additions session is not elevated)
# RDP/console → elevated PowerShell:
Z:\byovd\driver3\host-b.ps1 -Profile rust -Execute -ForceMap -SkipProve
```

`-Execute` from host tries `schtasks /RL HIGHEST`; it often **fails with Access denied** unless the guest task was pre-seeded at install.

Elevated gate (same as `scripts\lane-d-preflight.ps1`):

```powershell
# Admin
cd ..\byovd
.\scripts\lane-d-preflight.ps1 -Game rust
# exit 2 → driver3 Host A; map only on Host B
```

---

## Tiers (driver3 ladder)

| Tier | Meaning |
|------|---------|
| **D0** | Plan / static — `lane-gate.ps1` without elevation |
| **D1** | Host A DMA tree + offsets manifest present |
| **D2** | Host B driver2 map + kernel proof in `lab_manifest.json` |
| **D3** | driver2 developer gate ≥90% on VM |
| **D3i** | + cheat inspector execute on VM |
| **D4** | **Not claimed** — live ranked kernel on game PC |

See [`TIERS.md`](TIERS.md).

---

## Files

| File | Role |
|------|------|
| **`drivers/README.md`** | **Per-title + vehicle + DMA route cards** (method, R/W, tiers) |
| `open-route-doc.ps1` | Print doc path for a profile |
| `lane-gate.ps1` | Lane D recommendation → `driver3_gate.json` |
| `everything.ps1` | End-to-end split-trust plan or execute |
| `host-a.ps1` | DMA / live-read host steps |
| `host-b.ps1` | driver2 lab steps |
| `sync-lab-manifest.ps1` | Patch `lane_d` in `docs/lab_manifest.json` |
| `ranked-play.ps1` | Ranked = Host A by default |
| `driver3_everything.json` | Machine-readable spec |

Spec: `driver3_everything.json`.
