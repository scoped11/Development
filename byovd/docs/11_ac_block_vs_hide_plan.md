# AC block vs hide — lab plan (this kit)

**Scope:** BYOVD lab on your own hardware. This is not a live-game bypass guide. If a **serious** anti-cheat **blocks** a known vehicle (RTCore, iqvw, Corsair, …), you **cannot** “hide” that same driver into invisibility while keeping kernel R/W on the **same** host with AC running. The plan below splits **unblock** (cannot fix with hygiene) from **trace reduction** (what this repo implements).

References: [vxcall BYOVD](https://vxcall.github.io/posts/how-game-hackers-load-kernel-drivers/), [Microsoft vulnerable driver blocklist](https://learn.microsoft.com/en-us/windows/security/application-security/application-control/app-control-for-business/design/microsoft-recommended-driver-block-rules), [Tulach mapped-driver detection](https://tulach.cc/detecting-manually-mapped-drivers/), [mapper trace checklist (community)](https://www.unknowncheats.me/forum/4623972-post16.html).

## Decision tree

```text
Can CreateFile(\\.\Vehicle) succeed AND sc start vehicle OK on this boot?
  NO  + game AC usermode/kernel active  → Tier 0 (do not map here)
  NO  + no AC                            → fix OS (HVCI/blocklist), vehicle path, or --driver fallback
  YES + no AC                            → Tier 1 map (--ac-safe / map-secure-rwtest.ps1)
  YES + AC running later                 → Tier 0 for production titles; Tier 1 only measures traces you accept risking
```

## Tiers

| Tier | Situation | Action | Kit entry points |
|------|-----------|--------|------------------|
| **0 — Blocked** | AC or policy denies vehicle load/open | **Do not** map on that host with that `.sys`. Use snapshot VM **without** game AC; or lane C (dumps/offline RE). | `scripts/ac-preflight.ps1`, `mapper_nal.exe --ac-preflight` |
| **1 — Stealth map (lab)** | Vehicle works; you want **fewer** load/history artifacts, no AC IOCTL spray | Hygiene before `DriverEntry`, RwTest IOCTL wrap, no vehicle STOP, no DKOM unlink default | `--ac-safe`, `map-secure-rwtest.ps1`, `--quiet` |
| **2 — Measure** | Prove what artifacts remain | Lab I before/after (`docs/06_better_research_method.md` Phase 2), manifest JSON | `docs/lab_manifest.json` |
| **3 — Experiments** | One hypothesis per snapshot | Single Betest IOCTL family; **never** stack `--full` on first map | `05_existing_bypass_catalog.md` |

## Tier 0 — When the driver is **blocked** (no hide path)

Blocking types:

| Type | Symptom | “Hide” works? |
|------|---------|----------------|
| **Hash/name deny** | AC or filter driver refuses known `RTCore64.sys` / device | **No** — identity is the block |
| **CreateFile hook** | `\\.\RTCore64` fails only when AC is up | **No** — preflight must fail closed (`--ac-safe`) |
| **OS blocklist / HVCI** | `StartService` fails with CI policy | **No** — disable only in **lab VM** you own; not “hide” |
| **Disk / ASR** | Cannot copy/start `.sys` | **No** — different lane |

**Lab workflow (Tier 0):**

1. Run preflight (admin):

   ```powershell
   .\scripts\ac-preflight.ps1 -Driver rtcore
   ```

2. If exit code `2` (AC present + vehicle not openable): clone **VM snapshot**, **no** EAC/BE/Vanguard/ACE client, map there.
3. Record `docs/lab_manifest.json` on **both** hosts if you compare.
4. Optional: map on physical PC **before** installing AC **only** for Phase 1 stability — still expect runtime rescans if you launch a protected title later.

## Tier 1 — **Trace reduction** while vehicle stays loaded

When the vehicle **can** load, “hide” in public research means **scrub bookkeeping**, not unload:

| Step | What | Why |
|------|------|-----|
| 1 | Map `RwTest.sys` with `--ac-safe` | Same as `--quiet`: stealth `0x3f`, `--rw-only`, hygiene on, DKOM unlink off |
| 2 | Hygiene **before** payload `DriverEntry` | PiDDB, CI hash bucket, WdFilter, name zeroing, reg scrub without STOP (`vehicle_hygiene.hpp`) |
| 3 | IOCTL **wrap** on vehicle dispatch | No separate `\Device\Cheat` for R/W (`RwTestDrv.c`) |
| 4 | Vehicle fallback order | rtcore → corsair → ts → siv → trixx → nal if primary blocked at **device** layer only |
| 5 | After success | **Never** `sc stop` vehicle; reboot to clean; delete temp copies of `.sys` in user session if you care about disk forensics |
| 6 | Overlay | PID-bound session (`0x870`); optional `0x808` delete symlink on Betest **after** handle held |

**Command:**

```text
bin\mapper_nal.exe bin\RwTest.sys --driver rtcore --no-auth --ac-safe
```

Or:

```powershell
powershell -File map-secure-rwtest.ps1
```

## Tier 2 — Forensics (know what AC *could* still see)

After Tier 1 map on a **no-AC** VM, capture:

- Vehicle still in kernel memory and `PsLoadedModuleList` (unless you explicitly `--dkom-unlink` — PG risk).
- Patched `MajorFunction` on vehicle `DRIVER_OBJECT`.
- Executable pool for manual-mapped payload (not a listed PE module).
- Community AC checks: big pool, import stubs outside modules, stack walks ([Tulach](https://tulach.cc/detecting-manually-mapped-drivers/), [EMACLAB integrity notes](https://crvv.dev/emac-anticheat-driver-part6/)).

Deliverable: Phase 2 in `docs/06_better_research_method.md` → `docs/lab_i_notes.md` when you run it.

## Tier 3 — What we **do not** do on a blocked vehicle

- Patch or re-sign `RTCore64.sys` to “look different” (breaks trust chain).
- Unload vehicle to “delete” it (BSOD — see `docs/02_architecture.md`).
- `--full`, `--auto`, `--eac-be`, `--ace-vgk`, `--hide-all` as a response to **block** (adds kernel signal; does not remove deny list).
- Expect PiDDB scrub alone to bypass hash-based **loaded-module** scans.

## Mapper / script hooks (implemented)

| Tool | Behavior |
|------|----------|
| `--ac-preflight` | Log AC processes, HVCI/blocklist registry, vehicle device open test; exit `0` ok, `1` vehicle missing, `2` AC+no device |
| `--ac-safe` | Quiet map profile; if AC processes found and vehicle device never opens (after setup + fallback), **abort** unless `--force-map` |
| `--force-map` | Override `--ac-safe` abort (lab only) |
| `scripts/ac-preflight.ps1` | Same checks without mapping |

## Weekly cadence (blocked vehicle scenario)

| Day | Task |
|-----|------|
| 1 | `ac-preflight.ps1` on game PC → confirm Tier 0 vs 1 |
| 2 | Tier 0: map on no-AC VM; Phase 1 `process_rw.exe` |
| 3 | Tier 2: one baseline/post capture |
| 4 | Lane C game RE (no kernel) |
| 5 | Optional: one Tier 3 IOCTL on **cloned** snapshot only |

## If Tier 0 is permanent (e.g. Rust + EAC)

Use **Lane D** — do not iterate on hide IOCTLs: `docs/12_lane_d_split_trust.md` and `scripts/lane-d-preflight.ps1`.

## Related docs

- Architecture / no STOP: `docs/02_architecture.md`
- Detection surfaces: `docs/04_detection_surfaces.md`
- Research method: `docs/06_better_research_method.md`
- Vehicles: `vehicles/README.md`
- Hygiene vs kdmapper: `reference/TRACE_SURFACES.md`
