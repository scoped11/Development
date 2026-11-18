# Captures 2026-11-18

Owned, Defender-clean BattlEye samples:

- **Usermode** binaries from Sam’s Unturned BattlEye install (`BEService*.exe`, client/server DLLs). Authenticode **Valid**, BattlEye Innovations e.K.
- **Kernel** `BEDaisy.sys` from the **offline copy** at `C:\Users\bj284\Downloads\TraeTesting\cursor\Reversing\BEDaisy.sys` (not from live Program Files BattlEye; `BEService` left **Stopped**). SHA-256 `E7BCF344…`. WHQL leaf is Microsoft Hardware Compatibility Publisher; nested PKCS#7 names BattlEye Innovations e.K. Guest working copy: `C:\re\artifacts\BEDaisy.sys` (hash-matched).

See `hashes.json` and `docs/re-notes-be.md` / `docs/re-notes-eac.md` (Verified kernel facts). PE bodies (`*.exe` / `*.dll` / `*.sys`) are gitignored.

Afternoon static pass (no auto-analyze on BEDaisy): `kernel-facts-pm.json`, host objdump of INIT/`.text`/EOS FLT stubs, packed-name inventory. Guest Ghidra 12.1: `-import -noanalysis` works; Jython post-scripts do not (use Java).

Evening static VM-interpreter map (drivers **not** loaded): `vm-interpreter-summary.json`, `vm-interpreter-map.json`, `vm-interpreter-follow.json`, plus objdump of EOS handler bodies / BEDaisy SECTION.6 `0x1417c4ba1`. Guest artifact hashes re-checked on `AI_workspace` via `Z:\`.

Guest share letter is `Z:\`.
