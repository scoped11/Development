# Rust (EAC) + R6 (BE) — research plan (after greenfield close)

**Status:** Greenfield BYOVD on the gaming PC is **closed** (RTCore = blocked class; SI/KPH = gated Flt port; OEM queue exhausted). This plan picks **one architecture** and runs it in order.

**Live (no DMA):** **dispatch hijack** on an already-loaded non-AC driver; 0-day is only the kR/kW plug. See [`22_live_hijack_method.md`](22_live_hijack_method.md).

**Not goals:** Another public LOL vehicle on the main hunt list; stacking `--full` without a `primitive_id` or Lab I row.

**Exploit track:** [`21_lane_e_exploit_test_track.md`](21_lane_e_exploit_test_track.md) — acceptance levels **E0–E4**, manifest `lane_e`, E-AC allowed in your test policy.

---

## Architecture (three lanes)

```text
Lane D — Host A (EAC/BE):     dma/ live reads          [known good]
Lane E — test host (E-VM/E-AC): LPE / hijack / 0-day   [no blocked .sys load]
Lane A — Host B VM:            BYOVD --rw-only         [Lab I, IOCTL reference]

Bridge: build_id + lab_manifest (lane_d + lane_e)
```

Refs: `12_lane_d_split_trust.md`, `21_lane_e_exploit_test_track.md`, `14_eac_cookbook.md`.

---

## Phase 0 — Baseline inventory (½ day)

| # | Task | Command / artifact |
|---|------|---------------------|
| 0.1 | Confirm Tier 0 on game PC | Admin: `byovd\scripts\lane-d-preflight.ps1 -Game rust` and `-Game r6` |
| 0.2 | AC preflight RTCore | `byovd\scripts\ac-preflight.ps1 -Driver rtcore` |
| 0.3 | Driver inventory frozen | `driver-lab\scripts\enumerate-local-drivers.ps1` → keep dated CSV |
| 0.4 | Close greenfield log | Pointer: `reverse/*/notes/PASS*.md`, `19_greenfield_vehicle_hunt.md` |

**Exit:** Preflight exit **2** (or equivalent) on Host A → **Lane D is mandatory** for live work.

---

## Phase 1 — Static truth + manifest (1–2 days)

| # | Task | Done when |
|---|------|-----------|
| 1.1 | Rust dump for current build | `dumps/<date>/` + script.json or Il2Cpp index |
| 1.2 | Sync DMA offsets | `dma/scripts/` validators pass on dump |
| 1.3 | Write `build_id` | `byovd/docs/lab_manifest.json` → `lane_d.build_id` + hash |
| 1.4 | R6 parity (if in scope) | Same pipeline or note “Rust-only until dump exists” |

**Exit:** Host B tests can cite same `build_id` as Host A overlays.

---

## Phase 2 — Lane D (DMA) — **skip deep work if already green**

| # | Task | Notes |
|---|------|--------|
| 2.1 | `build_id` + offset sync only | Manifest + validators |
| 2.2 | Spot-check one struct after patch | Regression, not discovery |

**Exit:** Manifest cites current Rust build; DMA unchanged.

## Phase 2E — Lane E (exploit) — **parallel**

Follow **`21_lane_e_exploit_test_track.md`**: E-VM → E0/E1 → port to Betest-like R/W → E-AC with `primitive_id` + BFD notes.

**Exit:** `lane_e.levels_passed` includes at least **E2** on your chosen test profile, **or** dispatch hijack proof on E-VM.

---

## Phase 3 — Host B kernel proof (BYOVD lab) (3–5 days)

| # | Task | Notes |
|---|------|--------|
| 3.1 | VM snapshot “clean” | HVCI policy recorded |
| 3.2 | VM parity: **prove → ioctl → bypass** via `setup-vm-lab-betest.ps1` | Full Betest surface in VM; `docs/26_vm_kernel_driver_lab.md` — not Host A |
| 3.3 | `examples/process_rw.cpp` smoke | Target: notepad or lab PID |
| 3.4 | Lab I table (first pass) | Fill rows from `04_detection_surfaces.md` on **your** build |
| 3.5 | Same offset read via IOCTL | Prove parity with Phase 1 field (different host, same `build_id`) |

**Exit:** Lab I v0.1 — “after RW-only map, these artifacts appear / these do not.”

---

## Phase 4 — TH2 bridge (optional, 2–3 days)

Only if you run **real EAC/BE services on Host A** while mapping on **replica VM**:

1. Read `15_test_ac_replicas.md` + `16_timed_ac_harness_method.md`.
2. Map on replica with `map-ac-replica-lab.ps1` window — **never** on Host A.
3. Timed `process_rw` while replica AC profile matches Rust or R6.

**Exit:** Evidence that “AC family up on A, kernel test on B” does not collapse into one lie.

---

## Phase 5 — Fork (pick **one**, defer the other)

| Fork | When | Cost |
|------|------|------|
| **5A BFD deep** | You care *what* AC blocks, not reads | Method 18 deltas Rust vs R6; no new drivers |
| **5B Resident AC driver (not ACE)** | **EAC EOS** on Rust / **BEDaisy** on R6 — already loaded *with* the title. ACE-GAME is Tencent; **cannot** start Rust/R6 with ACE up | Clone + IDA only; no `sc start` of ACE |
| **5C Hygiene experiment** | Lab I shows **one** dominant artifact (e.g. PiDDB) | Single flag in VM per `06_better_research_method.md` |

**Default:** **5A** if DMA works; **5C** only after Lab I row is filled.

---

## Weekly rhythm

| Week | Focus |
|------|--------|
| 1 | Phase 0–1 |
| 2 | Phase 2 (DMA) |
| 3 | Phase 3 (VM map + Lab I) |
| 4 | Phase 4 or 5A; refresh dump if game patched |

---

## Stop rules

- Game patched → re-run Phase 1 before any live read claims.
- Preflight flips to “vehicle works on A” → still **prefer Lane D** for Rust/R6 ranked; Tier 1 is not permission to `--full`.
- Greenfield urge → run `greenfield-driver-candidates.ps1` for inventory only; **IDA queue is closed** unless fork **5B** explicitly chosen.

---

## First three commands (start Monday)

```powershell
# Admin PowerShell on game PC
cd C:\Users\baile\OneDrive\Desktop\Development\byovd
.\scripts\lane-d-preflight.ps1 -Game rust
.\scripts\ac-preflight.ps1 -Driver rtcore

cd ..\driver-lab\scripts
.\enumerate-local-drivers.ps1
```

Then: current Rust dump → sync offsets → `lab_manifest.json` `build_id`.
