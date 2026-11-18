# Captures 2026-11-18

Owned, Defender-clean BattlEye samples:

- **Usermode** binaries from Sam’s Unturned BattlEye install (`BEService*.exe`, client/server DLLs). Authenticode **Valid**, BattlEye Innovations e.K.
- **Kernel** `BEDaisy.sys` from the **offline copy** at `C:\Users\bj284\Downloads\TraeTesting\cursor\Reversing\BEDaisy.sys` (not from live Program Files BattlEye; `BEService` left **Stopped**). SHA-256 `E7BCF344…`. WHQL leaf is Microsoft Hardware Compatibility Publisher; nested PKCS#7 names BattlEye Innovations e.K. Guest working copy: `C:\re\artifacts\BEDaisy.sys` (hash-matched).

See `hashes.json` and `docs/re-notes-be.md` (Verified kernel facts). PE bodies (`*.exe` / `*.dll` / `*.sys`) are gitignored.

Guest share letter is `Z:\`.
