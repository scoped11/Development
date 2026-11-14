# Lane D — Split-Trust Calibration Bridge (new method)

> **Summary:** Lane D is **not** another kernel hide on the Rust machine. When EAC blocks BYOVD there, you **do not map on that session**. You split work across two trust boundaries and sync on **build ID + offsets**.

Use this when **Rust / EAC blocks BYOVD on the game PC** (Tier 0 in `11_ac_block_vs_hide_plan.md`). Lane D is **not** another kernel-hide trick. It **splits trust** so you never need a vulnerable driver on the same session as live anti-cheat.

```text
┌─────────────────────────────┐     ┌──────────────────────────────┐
│  Host A — game + EAC (Rust) │     │  Host B — lab VM, no AC      │
│  Live reads: DMA (Leech/VMM)│     │  Kernel proof: BYOVD+RwTest │
│  Static: Il2Cpp / dumps     │     │  IOCTL smoke + offset tests  │
└──────────────┬──────────────┘     └──────────────┬───────────────┘
               │         shared build manifest         │
               └──────── offsets.json + build_id ───────┘
```

## Why this is a different method

| Old approach | Lane D |
|--------------|--------|
| Same machine: map + play | **Two boundaries**: hardware DMA vs kernel lab |
| Scrub PiDDB so AC “forgets” RTCore | AC still sees vehicle if loaded on game PC — **don’t load there** |
| Rotate public LOL drivers | Same list class; Rust rescans memory anyway |
| `--full` / more kernel patches | More signal; does not fix block |

Public EAC discussion emphasizes **stack walks, non-module code, module integrity** over list scrub alone. Lane D **avoids live kernel tamper on the Rust box** while keeping a **verified read path** on a lab host.

## Prerequisites

| Piece | Location |
|-------|----------|
| BYOVD kit | `byovd/` — map only on Host B |
| DMA overlay | `Development/dma/` — live reads on Host A |
| Offset sync | `dma/scripts/validate_offsets_against_dump.py`, `sync_typeinfo_offsets.py` |
| Gate script | `byovd/driver3/lane-gate.ps1` · elevated `scripts/lane-d-preflight.ps1` |
| SDK entry | `byovd/driver3/` (**driver3**) |

## Workflow

### Step 0 — Gate

```powershell
cd byovd
.\scripts\lane-d-preflight.ps1 -Game rust
```

- Exit **0**: BYOVD device open, no AC → you may still use Tier 1 on this host (optional).
- Exit **2**: AC + no vehicle → **use Lane D** (this doc).
- Prints **Kernel DMA Protection / IOMMU** hints for Host A.

### Step 1 — Static truth (either host)

1. Dump / Il2Cpp for Rust build `B` → `Development/dumps/<date>/`.
2. Refresh `dma/src/engine/offsets.h` via existing DMA scripts.
3. Record `build_id` / dump hash in `docs/lab_manifest.json` → `lane_d.build_id`.

### Step 2 — Kernel proof (Host B only)

On VM snapshot **without** EasyAntiCheat:

```powershell
powershell -File map-secure-rwtest.ps1
bin\process_rw.exe --emit-manifest
```

Append stdout JSON into `lab_manifest.json` under `lane_d.kernel_proof` (marker read proves IOCTL path on build `B` or matching Windows build).

### Step 3 — Live reads (Host A, Rust PC)

1. Build DMA: `dma/scripts/build-windows.ps1`.
2. Run overlay with **same** `build_id` / offsets as Step 1.
3. If reads disagree with dump-derived layout → fix offsets on **static** side first (lane C), not kernel hide.

### Step 4 — Cross-check (optional)

On Host B, attach to an **unprotected** process with the same struct layout (test harness or offline clone). Compare one field at a known RVA with DMA read on Host A when both target build `B`. Log delta in `docs/lab_i_notes.md`.

## When Rust “blocks my driver”

| Symptom | Lane D action |
|---------|----------------|
| `ac-preflight` exit 2 with Rust/EAC running | Do **not** `--force-map` on game PC; use DMA on Host A, BYOVD on Host B |
| Map works until Rust starts | Runtime EAC scan — treat as Tier 0 for kernel; keep DMA for session |
| HVCI forced on later | Host A may block LOL drivers at OS level too — DMA unaffected if IOMMU allows |
| Only need structs for RE | Skip kernel entirely — lane C only |

## What we still do not claim

- Undetected Rust play with any kernel mapper.
- DMA bypass of server-side or behavioral AC.
- Single-machine “hide” when hash block is active.

## Files to maintain

```json
{
  "lane_d": {
    "method": "split-trust-calibration-bridge",
    "game": "rust",
    "build_id": "steam-changenumber-or-dump-hash",
    "kernel_proof_host": "lab-vm-1",
    "live_read_host": "game-pc-1",
    "dma_iommu_ok": true,
    "kernel_proof": { "marker_ok": true, "os_build": "10.0.26200.9974" }
  }
}
```

Merge template: `docs/lab_manifest.json`.

## Related

- `docs/06_better_research_method.md` — lanes A/B/C; Lane D extends the table.
- `docs/11_ac_block_vs_hide_plan.md` — Tier 0 triggers Lane D.
- `dma/README.md` — overlay build.
