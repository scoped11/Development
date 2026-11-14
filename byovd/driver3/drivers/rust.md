# Rust (EasyAntiCheat EOS)

Profile: `rust` · AC: **EAC EOS** · Profile file: `driver2/profiles/eac-rust.json` · Dump: `driver2/dumps/rust-eac-eos_*`

## Method

**Lane D (driver3):** Live Rust with EAC on the game PC is usually **Tier 0** for BYOVD — use **[DMA Host A](route-dma-host-a.md)** for session reads. Kernel **EAC-shaped** IOCTL proof runs on **Host B VM** with `eac-rust` profile (replica or clean snapshot).

Static RE: co-located dump includes `EasyAntiCheat_EOS.sys` — see `driver2/EAC_INTEGRATION.md`.

## Route

**Ranked (Host A):**

```text
RustClient + EasyAntiCheat_EOS (usermode)
  → dma/ overlay (build_id synced)
  ✗ no Driver2 map on this session
```

**Lab (Host B):**

```text
RTCore (vehicle) → mapper_nal --driver2 --ac-replica rust ...
  → Driver2.sys → \\.\MsFlt*
  → map-time: 0x844 EAC improve, 0x851 encptr (profile flags)
```

Gate: `driver3\lane-gate.ps1 -Game rust` · VM: `driver3\host-b.ps1 -Profile rust -Execute -ForceMap`

## Read / write

| Host | Route | API |
|------|-------|-----|
| **A — live** | DMA | Guest RVA → physical read in `dma/` engine; sync offsets with dump |
| **B — lab** | Driver2 | `0x870` auth → `0x805`/`0x806` any PID (e.g. `RustClient`); client: `Driver2Client pread <pid> <va> <n>` |
| **B — kernel** | Driver2 | `0x802` / `Driver2Client kread` (lab only) |

EAC-specific **map-time** IOCTLs (not for overlay R/W): `0x844`, `0x851` — see `IOCTL_REFERENCE.md`.

## Tiers

| Ladder | Level | This title |
|--------|-------|------------|
| AC block | **T0** | EAC running + vehicle blocked → DMA only on game PC |
| AC block | **T1** | VM, no EAC — `map-secure-rwtest.ps1` / driver2 map |
| driver3 | **D0–D1** | Gate + DMA + `build_id` |
| driver3 | **D2** | `host-b` kernel proof → `lane_d.by_profile.rust` |
| driver2 | **D3/D3i** | `developer-approval-gate.ps1 -IncludeAce` includes rust; inspector on VM |
| **D4 / L3** | Live ranked kernel | **Not claimed** |

Replica lab: `docs/15_test_ac_replicas.md` profile `rust` · pf_diag expects **0x844, 0x851, 0x869**, strip.
