# driver2 — AC capture bundles (SDK)

All title captures ship **next to profiles** so inspectors and IDA use one folder.

| AC | Folder | Profile |
|----|--------|---------|
| **EAC (Rust EOS)** | `rust-eac-eos_2026-11-13_0204/` | `profiles/eac-rust.json` |
| **BattlEye (R6)** | `r6-battleye_2026-11-13_0214/` | `profiles/be-r6.json` |
| **ACE (Delta Force)** | `deltaforce-ace_2026-11-13_0215/` | `profiles/ace-deltaforce.json` |
| **ACE (Arena Breakout)** | `arenabreakout-ace_2026-11-13_0215/` | `profiles/ace-arenabreakout.json` |

**Canonical lab mirror:** `driver-lab/dumps/` (all titles, future stamps).

Refresh into driver2:

```powershell
.\driver2\sync-ac-dumps.ps1              # all four
.\driver2\sync-ac-dumps.ps1 -Family ace  # ACE only
```

---

### EAC (Rust)

```text
rust-eac-eos_*/
  manifest.json
  kernel/EasyAntiCheat_EOS.sys
  eac/EasyAntiCheat/...
```

### BattlEye (R6)

```text
r6-battleye_*/
  manifest.json
  usermode/BEService_r6s.exe
  battleye/game/BEClient_x64.dll
  (BEDaisy.sys often absent on disk — see manifest kernel_note)
```

### ACE (Delta / Arena)

```text
deltaforce-ace_* | arenabreakout-ace_*/
  manifest.json
  kernel/ACE-GAME.sys          ← IDA primary (game bundle)
  kernel/ACE-GAME_system32.sys
  kernel/ACE-BASE.sys, ACE-ADVT.sys
  anticheatexpert/...
```

Delta manifest calls out when **game `ACE-GAME.sys` ≠ system32** — use `kernel/ACE-GAME.sys` for title build RE.

---

### Ban / PC fingerprint / what AC hunts in drivers

**[`../AC_FINGERPRINTS.md`](../AC_FINGERPRINTS.md)** — per-family tables from these bundles.  
Machine index: `ac_fingerprints_index.json` (regen: `../build-fingerprint-index.ps1`).
