# Driver research & bypass index

## Methods (byovd)

| Doc | Topic |
|-----|--------|
| `../../byovd/docs/02_architecture.md` | Vehicle + payload map |
| `../../byovd/docs/11_ac_block_vs_hide_plan.md` | Tier 0/1 block vs hide |
| `../../byovd/docs/12_lane_d_split_trust.md` | Lane D Rust/R6 split |
| `../../byovd/docs/14_eac_cookbook.md` | Recipes R1–R7 |
| `../../byovd/docs/15_test_ac_replicas.md` | Replica prove / ioctl / **bypass** |
| `../../byovd/docs/05_existing_bypass_catalog.md` | IOCTL & mapper flags |

## Research (bible)

| Doc | Topic |
|-----|--------|
| `../../research/14_KERNEL_DRIVERS.md` | IRP, IOCTL, kernel basics |
| `../../research/15_BYOVD_AND_DRIVER_TRUST.md` | BYOVD trust model |
| `../../research/16_EAC_BE_KERNEL_RESEARCH.md` | EAC/BE artifact matrix |
| `../../research/10_ANTICHEAT_AND_SECURITY.md` | Defense layers |

## Local driver audit

| `20_rust_r6_research_plan.md` | Phased plan (DMA + VM BYOVD) |
| `21_lane_e_exploit_test_track.md` | Lane E — exploit/LPE structure (E0–E4) |
| `22_live_hijack_method.md` | **Live, no DMA:** dispatch hijack + 0-day plug |
| `24_vm_exploit_hunt.md` | **VM-only** fuzz, IOCTL, BYOVD, snapshot workflow |
| `25_hijack_host_vm.md` | **VM-B** hijack host rubric + guest probe workflow |
| `26_vm_kernel_driver_lab.md` | **VmLab** IOCTL driver (GH tutorial track, VM-only) |
| `27_vm_bypass_confidence.md` | VM **L2/L3** pass gates — EAC/BE parity (not live guarantee) |
| `../vm/README.md` | Headless **DriverLab-E-26200** VirtualBox lab |

| Doc / script | Action |
|--------------|--------|
| [17_local_driver_ida_audit.md](17_local_driver_ida_audit.md) | IDA MCP workflow + host triage |
| `../../byovd/docs/18_block_fingerprint_delta.md` | **BFD** — AC device delta + CI archaeology |
| `../../byovd/docs/19_greenfield_vehicle_hunt.md` | Hunt **non-LOL** drivers only |
| `../scripts/greenfield-driver-candidates.ps1` | Filter inbox targets |
| `../scripts/enumerate-local-drivers.ps1` | `driverquery` + dumpbin BYOVD tiers |
| `../scripts/dump-rust-eac.ps1` | **Rust** EOS kernel + `EasyAntiCheat/` bundle → `dumps/` |
| `../scripts/check-driver-blocklists.ps1` | LOLDrivers + Win11 CI + EAC/BE paid heuristics |
| [../tools/drivers/README.md](../tools/drivers/README.md) | Arsenal + `driverlab-crack` tool list |

## Scripts

| Script | Action |
|--------|--------|
| `../../byovd/map-secure-rwtest.ps1` | RwTest `--ac-safe` |
| `../../byovd/scripts/bypass-ac-replica-live.ps1` | Betest replica bypass |
| `../../byovd/scripts/lane-d-preflight.ps1` | Lane D gate |
