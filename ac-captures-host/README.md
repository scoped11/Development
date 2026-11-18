# Anti-cheat artifact staging (offline)

Place **owned** captures here for static RE on the guest. **Driver bodies belong on the VM** — copy from guest install paths into this folder on the share, then mirror to `C:\re\artifacts\` for Ghidra.

| Guest action | Path |
|--------------|------|
| Copy from install | `C:\Program Files (x86)\EasyAntiCheat_EOS\EasyAntiCheat_EOS.sys` |
| Share staging | `D:\hostdev\ac-captures-host\<date>\EasyAntiCheat_EOS.sys` |
| Ghidra working copy | `C:\re\artifacts\EasyAntiCheat_EOS.sys` |
| Alternate staging | `D:\ac\samples\` (optional; same files as `ac-captures-host\<date>\`) |

BattlEye: copy `BEDaisy.sys` after kernel install (`driver-lab\scripts\dump-title-anticheat.ps1`).

**Inventory (SHA256, on-disk status):** [docs/vm-driver-inventory.md](../docs/vm-driver-inventory.md).

Example layout:

```
ac-captures-host/2026-11-17/EasyAntiCheat_EOS.sys
ac-captures-host/2026-11-17/BEDaisy.sys
```

Lab PE smoke test (already on share): `byovd\bin\Betest.sys` — see [docs/rea-vm-smoke-test.md](../docs/rea-vm-smoke-test.md). Do not commit proprietary game binaries unless policy allows.