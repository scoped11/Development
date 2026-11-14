# Research (EAC / BattlEye / kernel)

This kit exists because **Easy Anti-Cheat** and **BattlEye** ship **kernel components**. Research question: *what kernel state would an AC-class sensor correlate after a BYOVD map?* — not *how to pass a live ban wave*.

## Read these

| File | Topic |
|---|---|
| [research/16_EAC_BE_KERNEL_RESEARCH.md](research/16_EAC_BE_KERNEL_RESEARCH.md) | EAC/BE kernel map, artifact table, Lab I |
| [research/15_BYOVD_AND_DRIVER_TRUST.md](research/15_BYOVD_AND_DRIVER_TRUST.md) | BYOVD definition, HVCI/blocklist, kdmapper audit vs this mapper |
| [research/14_KERNEL_DRIVERS.md](research/14_KERNEL_DRIVERS.md) | IRP/IOCTL, IRQL, Ob callbacks, BSOD lessons |
| [../research/10_ANTICHEAT_AND_SECURITY.md](../../research/10_ANTICHEAT_AND_SECURITY.md) | Defense in depth (bible) |
| [../reference/TRACE_SURFACES.md](../reference/TRACE_SURFACES.md) | One-page kdmapper vs us |
| [../reference/kdmapper/](../reference/kdmapper/) | Upstream mapper (PiDDB / hash bucket examples) |
| [12_lane_d_split_trust.md](12_lane_d_split_trust.md) | **Lane D** — when EAC blocks BYOVD on game PC: DMA live + lab kernel, `build_id` sync |
| [13_greenfield_eac_rust.md](13_greenfield_eac_rust.md) | Greenfield Rust + EAC: driver vs non-driver |
| [14_eac_cookbook.md](14_eac_cookbook.md) | **EAC cookbook** — method catalog, gates G1–G3, recipes R1–R6 |

## Lab I (isolated VM, no protected title)

1. Snapshot OS build, HVCI, vulnerable-driver blocklist.
2. Dump / WinDbg: `PsLoadedModuleList`, `MmUnloadedDrivers`, pool ranges.
3. `run-map-stable.cmd`.
4. Diff the same lists. Grade findings (heuristic vs fixture-verified).
5. Optional: `--eac` **or** `--ac-preempt` on a **second** snapshot — never mix with first baseline.

Stop if PatchGuard `0x109` or you would launch a live multiplayer client.

**Recommended workflow:** [06_better_research_method.md](06_better_research_method.md) (phased: manifest → stable RW → forensic diff → pick kernel vs DMA vs offline).

## Other lanes (do not conflate)

| Lane | Trust boundary |
|---|---|
| This folder | Kernel R/W via mapped payload |
| `Development\dma` | Physical DMA / second machine |
| Rob Run dumps / TypeInfo | Game layout, offline |

AC composes **kernel + usermode + server**. Kernel hygiene cannot answer server-side validation.

## Evidence rule

Publisher docs ≠ detection algorithm. “EAC checks PiDDB” stays **heuristic** until you see it on **your** dump.
