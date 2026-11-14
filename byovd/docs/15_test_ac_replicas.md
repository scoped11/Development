# Test AC replicas — Rust (EAC) & R6 (BattlEye)

**Scope:** **Your** test environment. Profiles in `ac_replica_profiles.json`.

**We do not guarantee bypass** of production Rust or R6, or of a **faithful** replica of their full kernel stack. Success here means **your replica’s documented pass criteria** (below), on a **snapshot VM** you can reset.

---

## Titles

| Profile | Live AC | Replica `mapper` profile |
|---------|---------|-------------------------|
| **rust** | EasyAntiCheat EOS | `--ac-replica rust` |
| **r6** | BattlEye (`BEDaisy`) | `--ac-replica r6` |

---

## Replica levels (design your harness to one level at a time)

| Level | What you simulate | How the kit responds |
|-------|-------------------|----------------------|
| **L1** | Block `\\.\RTCore64` / deny LOL driver name | **Tier 0** preflight exit 2, or map on host **without** replica during map |
| **L2** | Real EAC/BE usermode + driver on **lab VM** | **Prove** then **IOCTL** recipes below |
| **L3** | Live ranked | **Lane D** only — not covered by “replica bypass” |

---

## Recipes (test env)

### Prove — vehicle + R/W (both profiles)

Confirms map + hygiene + session under replica **or** no-AC snapshot.

```powershell
cd byovd
.\scripts\map-ac-replica-lab.ps1 -Profile rust -Mode prove
.\scripts\map-ac-replica-lab.ps1 -Profile r6  -Mode prove
```

Manual:

```text
bin\mapper_nal.exe bin\RwTest.sys --driver rtcore --no-auth --ac-safe --ac-replica rust
bin\process_rw.exe --emit-manifest
```

### Bypass — live-test harness (replica **L2**, treat like ranked)

**Betest.sys only.** Runs hygiene map + preempt callback strip + NMI `0x868` + title IOCTLs + EAC/BE max `0x869` + unified neutralize (see mapper log). **Snapshot VM** — expect BSOD risk on repeated runs.

**Order (important):**

1. Replica **stopped** (or vehicle not blocked)
2. Run bypass map
3. **Start** your Rust/R6 replica + test client
4. `process_rw.exe` must still read marker

```powershell
.\scripts\map-ac-replica-lab.ps1 -Profile rust -Mode bypass
.\scripts\map-ac-replica-lab.ps1 -Profile r6  -Mode bypass
```

If replica is already blocking the vehicle: stop replica, bypass, then start replica. Or `-ForceMap` (lab only).

Pass: map exit 0, log shows `Pre-emptive callback strip`, `EAC/BE max-tier`, `process_rw` OK **after** replica start (still **no** game client).

**One-liners (admin):**

```text
run-bypass-rust-replica.cmd
run-bypass-r6-replica.cmd
```

Or:

```powershell
.\scripts\bypass-ac-replica-live.ps1 -Profile rust
.\scripts\bypass-ac-replica-live.ps1 -Profile r6
```

Manual mapper:

```text
bin\mapper_nal.exe bin\Betest.sys --driver rtcore --no-auth --ac-replica rust --ac-replica-bypass
bin\mapper_nal.exe bin\Betest.sys --driver rtcore --no-auth --ac-replica r6 --ac-replica-bypass
```

What `--ac-replica-bypass` enables (Betest only): hygiene, stealth `0x3f`, preempt strip `0x81B`, NMI `0x868`, title EAC `0x851` **or** BE `0x852`, max bundle `0x869`, light `kstealth`/`behav` — see `pf_diag.tmp`.

### IOCTL — Betest lab bundle (replica **L2** only, lighter than bypass)

Sends **title-specific** IOCTL masks (EAC `0x851` / BE `0x852`, optional NMI `0x868`). **High risk** — one snapshot, no other flags.

```powershell
.\scripts\map-ac-replica-lab.ps1 -Profile rust -Mode ioctl
.\scripts\map-ac-replica-lab.ps1 -Profile r6  -Mode ioctl
```

Equivalent:

```text
bin\mapper_nal.exe bin\Betest.sys --driver rtcore --no-auth --stealth 3f --ac-replica rust --ac-replica-ioctl
bin\mapper_nal.exe bin\Betest.sys --driver rtcore --no-auth --stealth 3f --ac-replica r6  --ac-replica-ioctl
```

Optional max-tier test (PG/BSOD risk): add `--eac-be` on a **disposable** snapshot only.

---

## Pass criteria (define in your replica README)

Copy into `docs/lab_manifest.json`:

```json
"ac_replica": {
  "profile": "rust",
  "level": "L2",
  "prove_exit": 0,
  "ioctl_log_ok": true
}
```

| Mode | Pass |
|------|------|
| **prove** | `process_rw` returns 0; marker `0x1122334455667788` |
| **ioctl** | Map exit 0; `pf_diag.tmp` shows EAC or BattlEye IOCTL subsection with OK counts |
| **bypass** | Map exit 0; start replica only; `process_rw` marker OK with replica running |
| **fail closed** | Replica blocks device + AC procs → preflight **2**; stop replica, bypass, then start replica |

---

## Rust replica + production game PC

Even if **ioctl** passes on L2 VM, **live Rust** may still block BYOVD → **Lane D** (`12_lane_d_split_trust.md`).

R6 same: BE heartbeat/report paths differ from EAC.

---

## Gate before every run

```powershell
.\scripts\ac-preflight.ps1 -Driver rtcore
.\scripts\lane-d-preflight.ps1 -Game rust   # or document r6 in manifest only
```

---

## Real AC running + replica (TH2)

**TH2:** real EAC/BE stays **on** the game PC (**no map**). Replica VM does timed map then starts **test** AC. See `docs/16_timed_ac_harness_method.md`.

```powershell
.\scripts\run-timed-harness.ps1 -Profile rust
.\scripts\run-timed-harness.ps1 -Profile r6
```

## Signed-module replica (disk stays vendor-signed)

Borrowed identity on DriverLab-E: inbox victim (`wcifs` / `luafv`) stays **Valid** Microsoft Authenticode; payload is stomp memory only. **Not** a PKCS#7 graft onto `VmLabDrv.sys`.

```powershell
& Z:\driver-lab\vm\bg\guest-signed-module-replica.ps1 -Profile rust -Victim wcifs
```

Doc: `driver-lab/docs/31_signed_module_replica.md`. Lab CA load of **your** driver is `29_labforge_method.md` (different story).

## See also

- `16_timed_ac_harness_method.md` — **TH** method with AC running (no game)
- `14_eac_cookbook.md` — R1–R6, §0b
- `05_existing_bypass_catalog.md` — IOCTL detail
- `ac_replica_profiles.json` — machine-readable flags
