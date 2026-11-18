# Anti-cheat artifact staging (offline)

Place **owned** captures here for static RE on the guest. **Driver bodies belong on the VM** — copy from guest install paths into this folder on the share, then mirror to `C:\re\artifacts\` for Ghidra.

| Guest action | Path |
|--------------|------|
| Copy from install | `C:\Program Files (x86)\EasyAntiCheat_EOS\EasyAntiCheat_EOS.sys` |
| Share staging | `Z:\ac-captures-host\<date>\EasyAntiCheat_EOS.sys` |
| Ghidra working copy | `C:\re\artifacts\EasyAntiCheat_EOS.sys` |
| Alternate staging | `D:\ac\samples\` (optional; same files as `ac-captures-host\<date>\`) |

Guest share letter is **`Z:\`** (not `D:\`; `D:` may be the Guest Additions ISO).

BattlEye kernel: offline `BEDaisy.sys` from `TraeTesting\cursor\Reversing\` is staged at `ac-captures-host\2026-11-18\BEDaisy.sys` (gitignored body; hash in `hashes.json`). Unturned 2026-11-18 still did not drop a `.sys` under Program Files; that live tree was not used.

**Inventory (SHA256, on-disk status):** [docs/vm-driver-inventory.md](../docs/vm-driver-inventory.md).

Example layout:

```
ac-captures-host/2026-11-17/EasyAntiCheat_EOS.sys   # gitignored; hashes.json committed
ac-captures-host/2026-11-17/EasyAntiCheat.sys
ac-captures-host/2026-11-17/hashes.json
ac-captures-host/2026-11-18/BEService.exe           # gitignored UM; hashes.json + REA JSON committed
ac-captures-host/2026-11-18/BEDaisy.sys             # gitignored kernel; offline Reversing copy
ac-captures-host/2026-11-18/hashes.json
```

`BEDaisy.sys` SHA-256 `E7BCF344EEFA7A4EBEA9B048E376958A5AD711489249033FA480A7EC8B6C018E` is on the share as of 2026-11-18 12:25 (offline copy). Live Common Files path remains unused.

Lab PE smoke test (already on share): `byovd\bin\Betest.sys` — see [docs/rea-vm-smoke-test.md](../docs/rea-vm-smoke-test.md). Do not commit proprietary game binaries unless policy allows.