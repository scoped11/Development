# Greenfield — Rust + EAC (driver vs non-driver)

Starting from zero on **Rust with EasyAntiCheat**, use this decision tree. It does not promise a live bypass.

## Default recommendation

**Lane D — Split-Trust Calibration Bridge** (`docs/12_lane_d_split_trust.md`):

> Lane D is **not** another kernel hide on the Rust machine. When EAC blocks BYOVD there, you **do not map on that session**. You split work across two trust boundaries and sync on **build ID + offsets**.

| Host | Tooling | Role |
|------|---------|------|
| **Game PC** (Rust + EAC) | `Development/dma/` | Live reads (no BYOVD on that session) |
| **Lab VM** (no AC) | `byovd/` RwTest + `--ac-safe` | Kernel IOCTL proof only |
| **Either** | dumps + Il2Cpp + offset scripts | Shared `build_id` |

Gate:

```powershell
cd byovd
.\scripts\lane-d-preflight.ps1 -Game rust
```

Exit **2** → use Lane D; do not map on the game PC.

## If you only wanted one column

| Approach | On Rust PC with EAC | Lab / research value |
|----------|---------------------|----------------------|
| **Non-driver** (dumps + DMA) | Primary path for layout + live memory | High |
| **BYOVD driver** | **Avoid** (Tier 0 block) | Map on no-AC VM only |
| **Hide / scrub / rotate LOL driver** | Does not fix hash + memory scans | Low for Rust live |

## Doc chain

1. **`14_eac_cookbook.md`** — full recipe index (start here for EAC work)
2. `11_ac_block_vs_hide_plan.md` — Tier 0 vs Tier 1 hide (when vehicle *can* load)
3. `12_lane_d_split_trust.md` — full Lane D workflow (**R1** in cookbook)
4. `06_better_research_method.md` — lanes A–D table
5. `docs/lab_manifest.json` — record `lane_d.build_id` and `kernel_proof`
