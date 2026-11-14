# What “confident” means for EAC/BE (VM)

Nobody can **guarantee** production Rust or R6 ranked bypass from a repo. This kit defines **measurable gates** so you are confident the **same Betest ioctl + mapper flags** you intend for live research **work on your snapshot VM** before you touch Host A.

| Level | Meaning | Script |
|-------|---------|--------|
| **L1** | Vehicle opens; preflight sane | `ac-preflight.ps1` |
| **L2** | prove → ioctl → **bypass** map; `pf_diag` shows strip + **0x869**; `process_rw` marker | `vm-lab-parity-gate.ps1` |
| **L3** | Map first, **AC replica running**, `process_rw` still OK | `run-timed-harness.ps1 -Profile rust\|r6` |
| **Live** | Lane D / hijack — **not** proven by L2 alone | `12_lane_d_split_trust.md` |

## One-shot VM gate (Admin in guest)

```powershell
D:\hostdev\driver-lab\vm\vm-lab-parity-gate.ps1 -Profile both -ForceMap
```

**Pass:** `driver-lab\exploit\notes\vm-parity-gate.json` → `"all_pass": true` and `lab_manifest.json` → `vm_bypass_gate`.

**Checks per profile (rust + r6):**

1. `RwTest` prove map + marker read  
2. `Betest` + `--ac-replica-ioctl` + EAC **0x851** or BE **0x852** in `pf_diag`  
3. `Betest` + `--ac-replica-bypass` + strip + **0x869** in `pf_diag`

## Highest lab confidence (replica up)

After L2 pass on a **clean** snapshot:

```powershell
D:\hostdev\byovd\scripts\run-timed-harness.ps1 -Profile rust
```

Same for `r6`. That is the closest automated test to “AC is running and IOCTL R/W still works” without launching the game.

## If gate fails

| Symptom | Fix |
|---------|-----|
| preflight exit 2 | Stop replica AC procs; map before starting replica; or `-ForceMap` on VM |
| `pf_diag` missing strip/869 | Rebuild `.\byovd\build.ps1`; use **bypass** mode not `--rw-only` |
| `process_rw` fail | Bugcheck log `C:\crash`; revert snapshot |
| HVCI / blocklist | Guest must match `lab_manifest` (`hvci off`, blocklist off) |

## Live EAC/BE

Even with **L3** on a test VM, production may still block BYOVD on Host A → **Lane D** + future **dispatch hijack** (`22_live_hijack_method.md`). The gate proves **your bypass stack fires correctly**, not that Epic/Ubisoft will not detect you.
