# Method TH2 — Dual-host: real AC running + replica lab (Rust / R6)

**Goal:** Exercise the stack **as if live AC is already running**, using your **test replica** as the kernel/IOCTL lab, while **real** EasyAntiCheat (Rust) or BattlEye (R6) stays on the **game PC** — **no kernel map, no game launch from this kit**.

This is the only architecture that stays consistent with:

- Real EAC/BE **already up** (typical “game PC” state)
- Vehicle **blocked** or too risky to map on that session
- Replica still getting **Betest / hygiene / IOCTL** work

---

## Split (must not mix)

```text
┌─ HOST LIVE ─────────────────────────┐     ┌─ HOST LAB (your replica VM) ────────┐
│ Real AC: EAC EOS (Rust) or BE (R6)  │     │ Replica: same family, L2 harness     │
│ AC PROCESS RUNNING                  │     │ AC replica RUNNING after map window  │
│ NO BYOVD / NO mapper_nal            │     │ BYOVD + Betest --ac-replica-bypass   │
│ Live reads: DMA (dma/) only         │     │ process_rw + optional --update       │
│ Dumps / offsets: dumps/             │     │ Same build_id as dumps               │
│ Do NOT start Rust.exe / R6 from kit │     │ Do NOT start Steam games here either │
└─────────────────────────────────────┘     └──────────────────────────────────────┘
                      sync: lab_manifest.lane_d.build_id + ac_replica.profile
```

**Hypothesis:** Live AC running **does not** get a kernel vehicle. Replica **does** get the timed map + AC-up R/W test. Together they simulate “live AC on, still researching / validating R/W” without putting BYOVD under production EAC/BE.

---

## Host LIVE (real AC running)

| Rule | Detail |
|------|--------|
| AC **on** | Leave EasyAntiCheat_EOS / BEService as they are — treat as production-like |
| **No** `mapper_nal`, `map-secure`, replica bypass | Tier 0 on this box |
| Reads | `dma/` overlay **only** (Lane D Host A) |
| Offsets | `dump-and-sync` / TypeInfo — **not** from kernel IOCTL on this PC |
| Games | **Do not** start Rust or Siege from scripts; if you already have AC running without a title, leave it |

If DMA is unused this session, Host LIVE is still **observation-only** (AC running, no kernel).

---

## Host LAB (test AC replica)

Timed replica path (AC **off** only for the map window):

| Phase | Lab replica | Action |
|-------|-------------|--------|
| L1 | **Stopped** | `run-timed-harness.ps1` Phase 1–2: Betest `--ac-replica rust\|r6 --ac-replica-bypass` |
| L2 | **Start replica** (no game) | Replica EAC-like or BE-like **running** |
| L3 | Replica **on** | `process_rw.exe` |
| L4 | Replica **on** | `mapper … Betest.sys --update --ac-replica … --ac-replica-bypass` |
| L5 | Replica **on** | `process_rw` again |

Pass on LAB: L3 and L5 exit 0; replica harness does not fail **your** rules.

---

## One combined run (operator)

1. **LIVE:** Confirm real AC processes exist; **do not** map. Optional: start DMA overlay if you need live reads.
2. **LAB:** `.\scripts\run-timed-harness.ps1 -Profile rust` **or** `-Profile r6` (prompts: replica stop → map → replica start → R/W → refresh).
3. Write `docs/lab_manifest.json`:

```json
{
  "method": "TH2-dual-host",
  "host_live": { "ac": "eac|battleye", "kernel_map": false, "dma": true },
  "host_lab": { "profile": "rust|r6", "timed_harness_exit": 0 },
  "lane_d": { "build_id": "" }
}
```

4. Compare: DMA field on LIVE vs dump `build_id`; kernel proof JSON from LAB `process_rw --emit-manifest`.

---

## Profiles

| Title | LIVE AC | LAB `--ac-replica` |
|-------|---------|-------------------|
| Rust | EasyAntiCheat EOS | `rust` |
| R6 | BattlEye / BEDaisy | `r6` |

Do **not** run `--ac-replica-bypass` on LIVE.

---

## Why this hypothetically “works with real AC running”

- Production AC **is allowed to stay running** (your constraint).
- Production AC **never** sees RTCore/Betest on that kernel (no vehicle).
- Replica **does** see map + IOCTL + R/W with **its** AC running after L2 — that is the **test** of the bypass bundle.
- Offsets stay honest via dumps + optional DMA.

If you instead map BYOVD on LIVE while real EAC/BE is up, you are back to **Tier 0 / detection** — TH2 forbids that.

---

## Scripts

```powershell
# LAB only (admin)
cd byovd
.\scripts\run-timed-harness.ps1 -Profile rust
.\scripts\run-timed-harness.ps1 -Profile r6

# LIVE: never those scripts. Optional:
#   dma\scripts\build-windows.ps1  → overlay only
```

Loader (`driver-lab`): use **Rust/R6 replica bypass** only if you are on the **lab** VM. On the live game PC, do not click those buttons.

---

## Not claimed

- Ranked / official servers
- Kernel hide from live EAC/BE
- Replica ≡ production AC

---

## Related

- Lane D: `12_lane_d_split_trust.md` (same split; TH2 names LIVE vs LAB explicitly)
- Replica bypass: `15_test_ac_replicas.md`
- Cookbook: `14_eac_cookbook.md`
