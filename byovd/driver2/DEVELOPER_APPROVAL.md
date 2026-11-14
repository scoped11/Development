# driver2 — developer approval rubric

What **“passes developer approval”** means in this repo: the stack is **shippable as a lab / integration SDK**, with **measured** gates — **not** a warranty on live EAC/BE/ACE bans.

## Approval tiers

| Tier | Meaning | How to earn it |
|------|---------|----------------|
| **D0 — Build** | Artifacts compile | `byovd\build.ps1` + `driver2\build.ps1` |
| **D1 — Comm** | Map + reopen device + R/W | `Driver2Client probe` after bypass |
| **D2 — AC ioctl chain** | Strip + 0x869 + title IOCTL in log | `verify-pf-diag.ps1` |
| **D3 — Developer gate** | D1+D2 for **rust, r6, universal** | `developer-approval-gate.ps1 -Execute` (VM) |
| **D3i — Cheat inspector** | D3 + static SDK rubric + Tier 1 map | `cheat-inspector.ps1` then `-Execute -Inspector` |
| **D4 — Live ranked** | **Not claimed** | Lane D (DMA + hijack); no BYOVD on ranked |

**Target score:** **≥ 90%** on D3 checks → `developer_approval.json` → `"approved": true`.

## What we fixed for real devs

| Issue | Fix |
|-------|-----|
| Symlink deleted → **can’t reopen** `\\.\MsFlt*` | **Default: keep symlink**; optional `--driver2-hide-symlink` |
| Device race after map | **24× retry** open in runner + `Driver2OpenWithRetry` |
| Thin prove-only story | **Driver2Client** + **kernel/process R/W** API |
| One game only | **`universal`** profile (EAC+BE+ACE masks) |
| No objective bar | **developer-approval-gate.ps1** |

## What is still on the developer (honest)

- **Module stomp** — `Driver2Stomp.sys` + mapper `--stomp` (lab, `docs/STOMP_BYPASS.md`); ranked path unchanged until gated.
- **Hijack-only comm** — separate from stomp; not driver2 default.
- **Live AC while mapping** — Tier 0; devs still map on **VM** or accept risk.
- **Overlay integration** — you ship `driver2_api.h` in your cheat; we don’t ship ESP.

## Run (VM only, when you approve)

```powershell
.\driver2\cheat-inspector.ps1                     # S1 static only (safe)
.\driver2\developer-approval-gate.ps1              # rubric only
.\driver2\developer-approval-gate.ps1 -Execute -ForceMap -Inspector
.\driver2\cheat-inspector.ps1 -Execute -ForceMap  # S1+S2 single entry
```

Rubric: `CHEAT_INSPECTOR.md`, vehicle myths: `VEHICLE_STEALTH.md`.

Pass → integrate `sdk/driver2_api.h` like any commercial driver SDK.
