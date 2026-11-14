# A better research method (EAC / BE / BYOVD)

This is the method that matches **what you already built** (BYOVD kit + bible + game RE). It prioritizes **reproducible evidence** and **understanding detection surfaces**, not “max bypass on first map.”

**Runnable recipes:** [14_eac_cookbook.md](14_eac_cookbook.md) (phases here map to cookbook **G*** / **R*** recipes).

**If anti-cheat blocks the vehicle:** use `docs/11_ac_block_vs_hide_plan.md` and `scripts/ac-preflight.ps1` before Phase 1 — Tier 0 means map on a **no-AC VM**, not more kernel hide IOCTLs.

## Principle

**Separate three lanes** and never let them blur in one run:

| Lane | Tooling | Question |
|---|---|---|
| **A — Kernel map & R/W** | `byovd/` RW-only map, `process_rw`, IOCTL `0x805` | Can we read/write target process memory **without BSOD**? |
| **B — Forensics / threat model** | WinDbg, Lab I dumps, `docs/04`, kdmapper **reference** | What kernel artifacts exist **after** map? |
| **C — Game layout** | `Development/` dumps, TypeInfo, overlay (usermode or DMA) | Where is the data we care about (not how to hide from AC)? |
| **D — Split-trust** | `dma/` live + `byovd/` lab VM (`docs/12_lane_d_split_trust.md`) | Rust/EAC blocks vehicle on game PC — DMA on A, kernel proof on B, same `build_id` |

Lane **B** informs whether lane **A** needs engineering later. Lane **C** does not require lane **A** to succeed on every title (DMA is a different boundary).

## Method 18 — When static blocklists lie

Production AC does not publish its deny list. Use **Block fingerprint delta (BFD)** on your real host: compare device `CreateFile` results with vs without AC running, plus Code Integrity `.sys` history — **no** `sc start`. See [18_block_fingerprint_delta.md](18_block_fingerprint_delta.md) and `driver-lab/scripts/block-fingerprint-delta.ps1`.

## Phase 0 — Manifest everything (15 minutes per machine)

Before any map, write `docs/lab_manifest.json` (copy template below). AC research without HVCI/blocklist/OS build is not comparable across runs.

```json
{
  "host_id": "lab-vm-1",
  "os_build": "10.0.26200.x",
  "hvci": "on|off|unknown",
  "vulnerable_driver_blocklist": "on|off|unknown",
  "vehicle": "NalDrvIntel.sys",
  "mapper": "mapper_nal.exe",
  "payload_sha256": "<Betest.sys>",
  "mode": "rw-only",
  "date_utc": "2026-11-12"
}
```

## Phase 1 — Stability gate (required)

**Success criterion:** map completes, device opens, session auth OK, `process_rw.exe` reads local marker.

```text
.\build.ps1
copy vehicles\NalDrvIntel.sys bin\
.\run-map-stable.cmd
bin\process_rw.exe   :: after you build the example once
```

Do **not** pass `--eac`, `--battleye`, `--ac-preempt`, or `--full` until Phase 1 passes on **two** cold boots.

Log artifact: `%TEMP%\pf_diag.tmp` (attach to manifest).

**Why this is “better”:** Most “undetected” stacks fail here. A stable RW path is the only honest foundation for overlay or RE.

## Phase 2 — Forensics diff (Lab I core)

On a **snapshot VM** with **no protected game** running:

1. **Baseline dump** (or live debugger): note `PsLoadedModuleList` (vehicle visible?), `MmUnloadedDrivers` strings, any obvious pool regions if you already know Betest base from log.
2. **Map once** with RW-only.
3. **Post map**: same checks; save WinDbg command output to `docs/lab_runs/<date>-post.txt`.
4. **Optional second snapshot**: same with `--ac-preempt` **only** to compare *what extra IOCTLs change* — expect BSOD risk; not for daily work.

**Grade each finding** (from bible ch. 12):

- **Fixture-verified** — you captured before/after on this build.
- **Heuristic** — literature says AC might use PiDDB; you have not proved BE/EAC reads it on your box.
- **Unresolved** — need newer dump or symbol.

Deliverable: one table row per artifact in `16_EAC_BE_KERNEL_RESEARCH.md` §4 filled in for **your** OS, not kdmapper screenshots.

## Phase 3 — Choose *one* trust boundary for reads

For **reading game memory in research**, pick **one** primary path and document tradeoffs:

| Method | Best for research when… | AC-relevant note |
|---|---|---|
| **Kernel process IOCTL** (lane A) | Single PC lab, you control map, no extra hardware | Ob callbacks affect **usermode** `ReadProcessMemory`, not necessarily your driver read — but AC may still kernel-scan pool/driver |
| **DMA** (`Development/dma`) | Studying **IOMMU / Kernel DMA Protection**, external reader | Different detection class; compare to lane A in writeups |
| **Offline dump / attach** (lane C) | TypeInfo, structs, no live AC | No runtime AC at all |

**Better method:** Rob Run struct work stays in **lane C** on dumps; only use lane **A** when you need **live** validation of an offset, on a **non-protected** or offline process first.

Do **not** use live ranked multiplayer as the test harness.

## Phase 4 — AC “bypass” as controlled experiments (optional)

Treat Betest’s CTF IOCTLs as **hypothesis tests**, not a loadout.

**One IOCTL family per snapshot**, manifest records flag:

```text
mapper_nal.exe bin\Betest.sys --driver nal --eac 01
```

Hypothesis example: “Does `--eac 01` (single feature bit) change Ob callback behavior on a **synthetic** target?” — measure with your own callback test driver or public docs, not a live ban outcome.

**Catalog:** `05_existing_bypass_catalog.md`. **Stop rules:** PG `0x109`, bugcheck during map, or any production game client you do not own for RE.

## Phase 5 — Compare to kdmapper (reference only)

Use `reference/kdmapper/` + `TRACE_SURFACES.md` to answer:

> If we added PiDDB scrub **only**, which rows in our Lab I table would change?

Do **not** implement until Phase 2 shows that row is **non-empty on your build**. Many builds already leave multiple traces (vehicle still loaded, pool exec, device object).

**Better method than kdmapper parity:** implement **one** hygiene piece **only if** Lab I proves it is the dominant artifact **and** RW-only map stays stable.

## What is *not* a better method

- `--full` on first successful map.
- Stacking strip + DKOM + deep hide + EAC improve in one command.
- Judging success by “no ban” on a protected title (confounds server, behavior, reports).
- Treating dma + kernel cheat + PiDDB scrub as one undifferentiated “stealth stack.”

## Suggested weekly cadence (solo researcher)

| Day | Task |
|---|---|
| 1 | Manifest + Phase 1 stability |
| 2 | Phase 2 baseline/post capture |
| 3 | Update artifact table + one paragraph in `docs/lab_i_notes.md` |
| 4 | Lane C game/struct work (orthogonal) |
| 5 | Optional: one flagged IOCTL experiment on cloned snapshot |

## Where this connects to EAC/BE papers / bible

- **Defender view:** `10_ANTICHEAT_AND_SECURITY.md` (layers, not bypass).
- **Kernel view:** `docs/research/16_*.md` (artifact matrix).
- **Your stack audit:** `15_BYOVD_AND_DRIVER_TRUST.md` (what we do vs kdmapper).

**Better research output** for Kyle: a dated **Lab I report** with hashes and graded artifacts, plus a **stable RW client** using `betest_ioctl.h` — not a wider IOCTL surface.

## Next file to create when Phase 2 runs

`docs/lab_i_notes.md` — paste WinDbg snippets, manifest JSON pointer, “dominant artifact” one-liner. Empty until you execute Phase 2.
