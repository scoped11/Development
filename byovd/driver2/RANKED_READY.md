# driver2 — ranked ready (Rust + R6)

**Product mode:** `single_pc_kernel` — map on **this PC**, read with **Driver2Client** or your EXE.

Default play path is **ranked-safe** (`RANKED_SAFE.md`): `--ac-safe` + `--mode session` (no bypass sweep), Tier 0 fail-closed.

## Use now (elevated PowerShell)

```powershell
cd C:\Users\baile\OneDrive\Desktop\Development\byovd

# Preflight (safe)
.\driver2\ranked-ready.ps1 -PreflightOnly

# Ranked session — map + cheat + you launch game
.\driver2\ranked-ready.ps1 -Execute -Live -Title rust
.\driver2\ranked-ready.ps1 -Execute -Live -Title r6
.\driver2\ranked-ready.ps1 -Execute -Live -Title both
```

Status files:

| File | Meaning |
|------|---------|
| `driver2_ranked_ready.json` | Gates + profiles (**PREPARED** before live map) |
| `driver2_ranked_verify.json` | **`Driver2Client probe`** after map |
| `ranked_100_confirm.json` | **100% confirmed** (map + probe rust/r6) |

**100% sure (admin):**

```powershell
.\driver2\confirm-ranked-100.ps1 -Title both
```

**`tier: S`** only when: map log `exit=0`, **probe exit 0**, rust+r6 `build_id`, full stack.

## Read memory

```text
.\bin\Driver2Client.exe pread <RainbowSix|RustClient_pid> <va_hex> <bytes>
```

Or link `sdk/Driver2Session.hpp` in your cheat.

## Profiles (from your dumps)

| Game | Profile | Map |
|------|---------|-----|
| Rust | `eac-rust` | `Driver2Runner --profile rust --mode bypass --force-map` |
| R6 | `be-r6` | `Driver2Runner --profile r6 --mode bypass --force-map` |

## Honest line

**Ranked ready** = stack works on one PC. **Not** undetected / ban-proof on live EAC or BattlEye.
