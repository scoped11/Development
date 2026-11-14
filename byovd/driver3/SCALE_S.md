# Scale S — evidence-backed cheat-dev platform

**Scale S** is the bar **above Scale A (80–90%)**: same Lane D honesty (no undetected / no live ranked BYOVD), but every claim is **backed by artifacts**, not static rubric alone.

| Grade | Meaning |
|-------|---------|
| **A** | Scale A dashboard ≥80%, manifests, dumps |
| **S** | A + Host B map proof + kernel_proof in manifest + dynamic SDK gate + offset cross-check + full-title live_ready |

## S-tier checklist (all required)

| # | Evidence | Command / artifact |
|---|----------|-------------------|
| 1 | Per-title `build_id` from **latest** dumps | `patch-build-ids.ps1` |
| 2 | All titles gate Tier 0 | `gate-all.ps1` → `driver3_gates_all.json` |
| 3 | Host B executed on VM | `vm_hostb_execute.log` contains `host-b exit=0` |
| 4 | Kernel proof in manifest | `lane_d.kernel_proof` + `by_profile.*.kernel_proof` |
| 5 | Dynamic inspector or VM developer gate | `cheat_inspector.json` `dynamic.gate_exit=0` **or** `developer_approval.json` without `scale_a_static_merge` |
| 6 | Offset truth vs dump | `offsets_validation.json` from `validate-offsets-for-s-tier.ps1` |
| 7 | Live-ready all shipped profiles | `developer_nod.json` all `live_ready[].ok` |
| 8 | DMA ship path | `RobRunDMA.exe` **or** documented `dma_status=build_only` in manifest |

## One-shot (host)

```powershell
cd byovd\driver3

# Static + gates (no VM)
.\reach-scale-s.ps1 -AllTitles -SkipVm

# Full S attempt (VM running, elevated task registered)
.\reach-scale-s.ps1 -AllTitles -VmMap

.\build-s-tier.ps1 -AllTitles
```

Outputs: `s_tier.json`, `dashboard.json` (`s_tier_pct`), `release\s_tier_bundle_manifest.json`.

**Not claimed at S tier:** D4 live kernel on game PC, undetected, FPGA autodetect.
