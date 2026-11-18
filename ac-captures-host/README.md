# Anti-cheat artifact staging (offline)

Place **owned** captures here for static RE on the guest. **Driver bodies belong on the VM** — copy from guest install paths into this folder on the share, then mirror to `C:\re\artifacts\` for Ghidra.

| Guest action | Path |
|--------------|------|
| Copy from install | `C:\Program Files (x86)\EasyAntiCheat_EOS\EasyAntiCheat_EOS.sys` |
| Share staging | `Z:\ac-captures-host\<date>\EasyAntiCheat_EOS.sys` |
| Ghidra working copy | `C:\re\artifacts\EasyAntiCheat_EOS.sys` |
| Alternate staging | `D:\ac\samples\` (optional; same files as `ac-captures-host\<date>\`) |

Guest share letter is **`Z:\`** (not `D:\`; `D:` may be the Guest Additions ISO).

BattlEye: copy `BEDaisy.sys` after the kernel is actually dropped (`driver-lab\scripts\dump-title-anticheat.ps1`). Unturned 2026-11-18 installed packed `BEService.exe` only — no `.sys` on disk.

**Inventory (SHA256, on-disk status):** [docs/vm-driver-inventory.md](../docs/vm-driver-inventory.md).

Example layout:

```
ac-captures-host/2026-11-17/EasyAntiCheat_EOS.sys   # gitignored; hashes.json committed
ac-captures-host/2026-11-17/EasyAntiCheat.sys
ac-captures-host/2026-11-17/hashes.json
ac-captures-host/2026-11-18/BEService.exe           # gitignored UM; hashes.json + REA JSON committed
ac-captures-host/2026-11-18/hashes.json
```

`BEDaisy.sys` was **not** found on host or guest as of 2026-11-17 **or** after the 2026-11-18 Unturned BattlEye install.

Lab PE smoke test (already on share): `byovd\bin\Betest.sys` — see [docs/rea-vm-smoke-test.md](../docs/rea-vm-smoke-test.md). Do not commit proprietary game binaries unless policy allows.