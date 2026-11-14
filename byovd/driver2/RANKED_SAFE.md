# Ranked-safe (Rust / R6)

**Ranked-safe** is the **lab Tier 1** play path: fail-closed preflight, **`--ac-safe`**, **ioctl-only** map (`--mode session`), **no bypass sweep**, then `Driver2Client probe`.

It is **not** ban-proof or undetected on live EAC/BattlEye ranked. It reduces **avoidable mapper artifacts** vs full `everything` / bypass.

## Commands

```powershell
# Rust or R6 — admin, -Live when game AC is running
.\driver3\single-pc-play.ps1 -Game rust -Execute -Live -RankedSafe
.\driver3\single-pc-play.ps1 -Game r6  -Execute -Live -RankedSafe

# Map only
.\driver2\ranked-safe-map.ps1 -Profile rust -Live
```

Evidence: `driver2\ranked_safe.json`, `driver2_ranked_verify.json`, **`driver2_dump_tracking.json`** (dump IOCTL parity + no bypass sweep).

After map, **`Invoke-Driver2DumpTrackingApply`** checks your captures (`AC_FINGERPRINTS.md`):

| Title | Dump-driven session IOCTLs | Avoid on ranked-safe |
|-------|---------------------------|----------------------|
| Rust | `0x844`, `0x851` via map + `eac-improve` / `eac-encptr` | `0x869` bypass sweep, `0x81B` strip spray |
| R6 | `0x852` + `be-improve` | same |

Live EAC/BE `.sys` / `BEService` hashes are compared to `driver2/dumps/*/manifest.json` when files exist.

## Blocks (by design)

| Condition | Result |
|-----------|--------|
| Tier 0 (AC + vehicle won't open) | **Exit 2** — no map |
| Title AC running, no `-Live` | **Blocked** |
| Full bypass stack | Use `-UnsafeFullBypass` on `single-pc-play` (not ranked-safe) |

## Safer session order (when you can)

1. Boot / no game running, AC **not** started  
2. `ranked-safe-map.ps1` (vehicle Tier 1)  
3. Probe OK → launch cheat EXE → then start game  

Still not “safe” against kernel inspection while playing — honest tier labels only.

See `docs/11_ac_block_vs_hide_plan.md`.
