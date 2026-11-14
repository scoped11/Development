# Host lab, VM, and lane architecture (session record)

**Edition:** 13 November 2026 | Consolidates user goals and repo docs from this research arc.

## 1. User constraints (own Win11 build 26200)

| Constraint | Intent |
|------------|--------|
| **No DMA on live game path** | DMA stays lab/calibration; live read via other lanes |
| **No new LOL `.sys` on game PC** | No kdmapper/scrub on ranked host |
| **Dispatch hijack + kR/W primitive** | Lane E on **non-AC** resident driver |
| **VM for dangerous work** | Install, BYOVD, Betest map, hijack probes, parity gate |
| **Lab must match live AC surface** | Real EAC/BE/ACE captures + replica profiles, not IOCTL-only toy |

Titles in scope: **Rust (EAC EOS)**, **R6 (BE)**, **ARK SA (BE)**, **Delta Force (ACE)**, **Arena Breakout (ACE)**.

## 2. VM — VirtualBox `DriverLab-E-26200`

| Item | Detail |
|------|--------|
| ISO | `driver-lab/vm/media/Win11.iso` |
| Guest creds | `driver-lab/vm/lab.credentials.json` (gitignored) |
| Blocker fixed | VT-x disabled in BIOS → enabled |
| Install | Unattended; DVD-first boot order |
| Status (session end) | GA may still be pending; bootstrap via `wait-and-bootstrap-guest.ps1` |

Key scripts: `install-vm-unattended.ps1`, `vm-lab-parity-gate.ps1`, `probe-hijack-candidates.ps1`.

Docs: `driver-lab/docs/22_live_hijack_method.md`, `25_hijack_host_vm.md`, `24_vm_exploit_hunt.md`, `27_vm_bypass_confidence.md`.

## 3. Lane model (repo)

| Lane | Host | Method |
|------|------|--------|
| **D** | Game PC | Split-trust / DMA calibration bridge (`byovd/docs/12_lane_d_split_trust.md`) |
| **E** | Game PC (EAC/BE up) | Hijack **non-AC** driver + kernel R/W only — **not** Betest map on live box |
| **VM L2** | DriverLab clone | `map-ac-replica-lab` prove → ioctl → bypass + `pf_diag` |
| **VM L3** | — | Production session — **not** a pass target |

**ACE-GAME (Tencent):** Present on host for Delta/ABI titles; **cannot** substitute for Rust/R6 AC. Static IDA + `sc start` only on **VM**, not Rust host (`candidate-001-ace-game-static-hunt.md`).

## 4. Hijack chokepoint (live)

Documented XFG / dispatch focus: open resident **non-AC** driver (GPU/storage/filter), splice `IRP_MJ_DEVICE_CONTROL`, avoid EAC/BE/ACE/LOL devices.

Blocked families (probe regex): EasyAntiCheat, BEDaisy, BEService, ACE-GAME, iqvw/Nal/RTCore, dbk/kph.

## 5. Betest / VmLab split

| Component | Role |
|-----------|------|
| **Betest.sys** | Full IOCTL + AC-shaped lab payload |
| **RwTest.sys** | Minimal prove driver (`map-ac-replica` prove mode) |
| **VmLabDrv.c** | Optional hello IOCTL; **not** primary AC parity |

## 6. Confidence gates

| Level | Artifact |
|-------|----------|
| L2 | `vm-lab-parity-gate.ps1` → `vm-parity-gate.json`, `lab_manifest.vm_bypass_gate` |
| TH | `run-timed-harness.ps1` after L2, replica **running**, no game client |

## 7. Cross-references

- Captures: [17_HOST_AC_CAPTURES_2026-11-13.md](17_HOST_AC_CAPTURES_2026-11-13.md)
- Mapper profiles: [18_AC_REPLICA_MAPPER_INTEGRATION.md](18_AC_REPLICA_MAPPER_INTEGRATION.md)
- Driver ops: [20_SESSION_DRIVER_OPS_AND_STABILITY.md](20_SESSION_DRIVER_OPS_AND_STABILITY.md)
- Kernel AC theory: [16_EAC_BE_KERNEL_RESEARCH.md](16_EAC_BE_KERNEL_RESEARCH.md)
