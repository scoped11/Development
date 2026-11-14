# Cheat inspector — buyer / peer review bar

What a **strong-driver cheat developer** or **reseller inspector** checks before they pay or integrate. This repo answers with **artifacts + JSON scores**, not slogans.

**Pass bar**

| Phase | Target | Script | Output |
|-------|--------|--------|--------|
| **S1 — Static** | ≥ **90%** | `cheat-inspector.ps1` | `cheat_inspector.json` |
| **S2 — VM dynamic** | ≥ **90%** avg + all profiles | `cheat-inspector.ps1 -Execute -ForceMap` | chains `developer_approval.json` |
| **S3 — ACE pack** | same + Delta/Arena | `-IncludeAce` on both scripts | |

S1 runs **without** mapping (safe anytime). S2/S3 only on **DriverLab VM** when you approve live map.

Honest scope: **S1+S2 = lab SDK certified**. That is **not** live ranked ban immunity (`DEVELOPER_APPROVAL.md` D4).

---

## What inspectors ask (and how we answer)

### Load model

| Question | Expected answer in this kit |
|----------|----------------------------|
| Is your payload signed? | **No** — `Driver2.sys` is manual-mapped via BYOVD vehicle |
| Why does comm “feel” commercial? | Session `0x870`, `0x805`/`0x806`, `sdk/driver2_api.h` |
| Do you load via `sc create` on the cheat driver? | **No** — BYOVD map only (`DEVELOPER_SDK.md`) |

### Vehicle (RTCore / fallback)

| Question | Answer |
|----------|--------|
| RTCore blocked with AC up? | **Tier 0** — do not map; `ac-preflight.ps1` exit `2` |
| Can you hide RTCore from AC? | **Partial** — trace reduction (hygiene), optional DKOM unlink (**off by default**, PG risk) |
| Encrypt / spoof RTCore so blocklist misses? | **No product claim** — hash/deny is load-time; see `VEHICLE_STEALTH.md` |
| Vehicle after map? | Stays loaded; **no `sc stop`**; reboot to clean |

### Hide / encrypt / spoof — inspector script

Full matrix: **`VEHICLE_STEALTH.md`**. One line for buyers:

> **Block ≠ hide.** Spoofing the filename does not beat deny lists. Encryption targets **payload polish**, not “invisible RTCore.” Default stack is **Tier 1 lab**: hygiene on, DKOM unlink off, symlink kept.

### ACE / EAC / BE

| AC | Profile | Proof in `pf_diag` |
|----|---------|------------------|
| ACE | `deltaforce`, `arenabreakout` | `0x846`, `0x84E`, `0x86A` |
| EAC | `rust` | `0x851`, `0x869`, strip |
| BE | `r6` | `0x852`, `0x869`, strip |
| All | `universal` | combination + strip + `0x869` |

Captures: **`driver2/dumps/`** (EAC + BE + ACE bundled); lab mirror `driver-lab/dumps/`; host bundle `ac-captures-host/`.

### Comm / reopen

| Check | Pass criterion |
|-------|----------------|
| Symlink | Default **kept** — log contains `symlink kept` |
| Reopen | `Driver2Client probe` exit `0` after map |
| Retries | SDK + runner open retry (24×) |

### Trace reduction (what “hide” means here)

| Step | Default driver2? |
|------|------------------|
| PiDDB / CI bucket / WdFilter hygiene | **Yes** (`vehicle_hygiene.hpp`) |
| Mapper stealth `0x3f` with `--inspector` | **Yes** (`Driver2Runner --inspector`) |
| DKOM unlink RTCore from module list | **No** (opt-in; PG risk) |
| `--driver2-hide-symlink` | **No** (breaks reopen) |
| ENCRYPT live vehicle pages | **No** (mapper skips — BSOD risk) |

---

## Evidence bundle (attach for inspection)

After S2 pass, zip:

```text
developer_approval.json
cheat_inspector.json
%TEMP%\pf_diag.tmp          (last profile)
driver2\forensics\<run>\snapshot.json
bin\Driver2.sys             (hash in JSON)
IOCTL_REFERENCE.md          (pinned version)
```

`forensics-capture.ps1` runs automatically from `cheat-inspector.ps1 -Execute`.

---

## Commands

```powershell
# Static only (no kernel)
.\driver2\cheat-inspector.ps1

# VM — inspector Tier 1 map + full approval chain
.\driver2\cheat-inspector.ps1 -Execute -ForceMap

# ACE buyer pack
.\driver2\cheat-inspector.ps1 -Execute -ForceMap -IncludeAce
```

---

## Related

- `DEVELOPER_SDK.md` — integration
- `DEVELOPER_APPROVAL.md` — D0–D4 tiers
- `docs/11_ac_block_vs_hide_plan.md` — Tier 0 vs 1
- `VEHICLE_STEALTH.md` — hide / encrypt / spoof truth table
- `CONFIDENCE.md` — probability bands (honest)
- `AC_FINGERPRINTS.md` — what each AC likely fingerprints + driver signals (from `dumps/`)
