# DriverLab VM (Host B) — what you are doing and how to read/write

Inside **`DriverLab-E-26200`**, mount **`Z:\byovd`** (same tree as the host). This Windows has **no live game AC** on the session you map in — that is where **Driver2** runs.

## What the VM is for

| Job | Script | Notes |
|-----|--------|-------|
| Kernel map + IOCTL proof | `host-b.ps1 -Profile rust -Execute -ForceMap` | Per-title via `games.json` |
| Full stack | `driver2\everything.ps1 -Target vm -Execute -ForceMap` | Profile from route card |
| Developer gate | `driver2\developer-approval-gate.ps1 -Execute -ForceMap` | Fills `developer_approval.json` |
| Inspector (dynamic) | `driver2\cheat-inspector.ps1 -Execute -ForceMap -IncludeAce -IncludeBe` | `dynamic.gate_exit=0` for Scale S |
| Host automation | `vm-guest-hostb-execute.ps1` | Log → `vm_hostb_execute.log` |

**One-time (elevated in VM):**

```powershell
Z:\byovd\driver3\register-driver3-elevated-task.ps1
```

Then the **host** can queue work via `vm-request-hostb.ps1` / `vm-test-from-host.ps1 -Execute`.

## How to READ in the VM (kernel / process)

After map:

```powershell
cd Z:\byovd
.\bin\Driver2Client.exe pread <pid> <virtual_address_hex> <byte_count>
```

Or C++:

```cpp
Driver2Session drv;
drv.Connect();  // opens \\.\MsFltComm* after map
drv.ReadProcess(pid, va, buf, n);
```

**Kernel read (lab only):**

```text
Driver2Client kread <kernel_va> <n>
IOCTL 0x802
```

Offsets for game structs still come from **host dumps** — prove one marker against dump RVA in lab, then use same `build_id` on Host A DMA.

## How to WRITE in the VM

```powershell
.\bin\Driver2Client.exe pwrite <pid> <va> <hex_bytes>
```

IOCTLs `0x805` / `0x806` after session auth **`0x870`**. Title-specific map-time hooks (EAC `0x844`, ACE `0x846`, BE `0x852`) run at **map** — see capture folders `driver2/dumps/rust-eac-eos_*`, etc.

## Map commands (from your dumps)

**Rust (EAC EOS):**

```text
Driver2Runner.exe --profile rust --mode bypass --force-map
```

**R6 / BE:**

```text
Driver2Runner.exe --profile r6 --mode bypass --force-map
```

**ACE titles:** `deltaforce`, `arenabreakout` — see `drivers/*.md`.

## Artifacts the host pulls from `Z:\byovd\driver3`

| File | Meaning |
|------|---------|
| `vm_hostb_execute.log` | Must contain `host-b exit=0` for Scale S |
| `kernel_proof_<profile>.json` | Merged into `lane_d.kernel_proof` |
| `vm_guest_smoke.json` | Plan/smoke OK |

Host sync:

```powershell
# On Host A:
.\sync-guest-artifacts.ps1 -AllKernelProofs -IncludeInspector
```

## VM setup

```powershell
Z:\byovd\driver3\setup-vm-driver3.ps1
Z:\driver-lab\vm\setup-guest-remote.ps1   # elevated — SSH/WinRM + elevated task
```

If `vm-test-from-host -Execute` returns **24**, RDP to `127.0.0.1:3389` as **DriverLab**, open elevated PowerShell, run `register-driver3-elevated-task.ps1` once.
