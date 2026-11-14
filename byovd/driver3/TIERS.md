# driver3 tier ladder

driver3 measures **split-trust readiness**, not “undetected ranked.”

| ID | Bar | How |
|----|-----|-----|
| D0 | Know which host you are | `lane-gate.ps1` (plan OK without Admin) |
| D1 | Host A can read live | `dma/` built; `build_id` in manifest |
| D2 | Host B kernel proof | `host-b.ps1 -Execute` → `lane_d.kernel_proof` |
| D3 | SDK developer gate | `driver2\developer-approval-gate.ps1` ≥90% |
| D3i | Inspector tier on VM | `cheat-inspector.ps1 -Execute -Inspector` |
| D4 | Live ranked BYOVD | **Out of scope** — use DMA (Host A) only |
| **S** | Evidence-backed shop grade | [`SCALE_S.md`](SCALE_S.md) · `build-s-tier.ps1` · `s_tier.json` |

When `lane-d-preflight` exits **2**, treat this machine as **Host A** for that session.

**Scale A** = 80–90% SDK (`SCALE_A.md`). **Scale S** = Scale A plus VM Host B proof, manifest `kernel_proof`, dynamic inspector, offset validation — still **not** undetected / D4.

**Per-title detail** (which route, which IOCTL/DMA API, T0 vs D2): [`drivers/README.md`](drivers/README.md) and `drivers/<profile>.md` linked from `games.json` → `doc`.
