# REA + Ghidra — VM smoke test

**Scope:** Static pipeline proof only (no live anti-cheat).  
**VM:** `AI-Workspace` (Windows 11 26200.x)  
**Share:** host `C:\Users\bj284\Downloads\TraeTesting\cursor\Development` → guest `D:\hostdev` (or `Z:` / `\\vboxsvr\hostdev` when letter varies — see [vm-checklist.md](vm-checklist.md)).

## Path map

| Role | Path |
|------|------|
| Host Development tree | `C:\Users\bj284\Downloads\TraeTesting\cursor\Development` |
| VBox share name | `hostdev` |
| Guest preferred mount | `D:\hostdev` |
| Guest UNC | `\\vboxsvr\hostdev` |
| Ghidra (guest target) | `C:\Tools\ghidra_12.1_PUBLIC` (after [install-ghidra-rea-guest.ps1](../driver-lab/vm/install-ghidra-rea-guest.ps1)) |
| MCP config (guest) | `%USERPROFILE%\.cursor\mcp.json` |

## Doctor — host snapshot (2026-11-17)

Captured on **host** before guest Ghidra install (`npm exec rea-agents@latest -- doctor`). Guest doctor should be re-run after install and saved to `docs/rea-doctor-guest-snapshot.txt`.

```json
{
  "healthy": false,
  "identity.cli_package_version": "6.9.0",
  "scope_checks_summary": {
    "node": { "ok": true, "detail": "24.22.1" },
    "host": { "ok": true, "detail": "win32 x64" },
    "ghidra": {
      "ok": false,
      "detail": "GHIDRA_INSTALL_DIR is not set",
      "remediation": "Set GHIDRA_INSTALL_DIR to extracted Ghidra 12.x (>=12.1)"
    },
    "registration:cursor": {
      "ok": false,
      "detail": "foreign ownership on existing rea registration",
      "remediation": "Merge env from docs/mcp-rea-guest.example.json or re-run guest install script"
    }
  }
}
```

**Target guest doctor line:** `ghidra` / `providerInspections[ghidra].checks[configuration].ok` → **true**.

## Guest install (one shot)

1. **Log in** to `AI-Workspace` once (required for VirtualBox Guest Control and Cursor).
2. Confirm share: `dir D:\hostdev\docs\vm-checklist.md` (or `Z:\...`).
3. Elevated PowerShell:

```powershell
Set-ExecutionPolicy Bypass -Scope Process
D:\hostdev\driver-lab\vm\bootstrap-vm-tools.ps1
# or: D:\hostdev\driver-lab\vm\install-ghidra-rea-guest.ps1
npx.cmd rea-agents@latest setup   # select Cursor; approve MCP
npx.cmd rea-agents@latest doctor  | Tee-Object D:\hostdev\docs\rea-doctor-guest-snapshot.txt
```

Expected doctor shape: [rea-doctor-guest-expected.md](rea-doctor-guest-expected.md).

4. Restart **Cursor on the guest**.

## Safe analysis prompt (pipeline proof)

Use an **owned lab** PE on the share — not a live protected game binary.  
Inventory: [vm-driver-inventory.md](vm-driver-inventory.md).

**Primary sample (on share today):** `D:\hostdev\byovd\bin\Betest.sys`  
SHA256: `3BC356C9300ADC233D3FF264802856AA372FDB6515BF1103AA6E6DE70C0FD019` · x64 kernel driver · 171 520 bytes.

Optional: copy to guest local disk for Ghidra I/O:

```powershell
New-Item -ItemType Directory -Force C:\re\artifacts | Out-Null
Copy-Item D:\hostdev\byovd\bin\Betest.sys C:\re\artifacts\
```

**After** staging `EasyAntiCheat_EOS.sys` or `BEDaisy.sys` per inventory, repeat the same prompt with that path under `C:\re\artifacts\` (see [re-notes-eac.md](re-notes-eac.md) / [re-notes-be.md](re-notes-be.md)).

**Fallback (no share):** `C:\Windows\System32\notepad.exe` — proves `open_binary` only until Ghidra is set.

**Cursor chat prompt (guest):**

> Static documentation only. Open `C:\re\artifacts\Betest.sys` (or `D:\hostdev\byovd\bin\Betest.sys`) with REA, run `binary_overview` and `inspect_pe_load_config`, list image architecture and main sections, and name the driver entry point with evidence. Do not suggest injection, mapping, or anti-cheat evasion.

**Expected tools:** `open_binary` → `inspect_pe_load_config` → `binary_overview` (Ghidra backend once `GHIDRA_INSTALL_DIR` is set).

### Host partial run (no Ghidra — 2026-11-17)

| Step | Result |
|------|--------|
| `open_binary` notepad.exe | OK — `pe`, `x86_64`, sha256 `2d0b3948c1edc5c6a1d1f3e3658613252cfbbfa1b98b46cb7cc8e5bf2efb158f` |
| `binary_overview` | **Blocked** — `provider_unavailable` until `GHIDRA_INSTALL_DIR` configured |

Re-run the same prompt on the **guest** after install; append doctor OK output to this file.

## Host automation (blocked without guest session)

`VBoxManage guestcontrol` returned `VBOX_E_GSTCTL_GUEST_ERROR` when no interactive session was active. After Sam logs in:

```powershell
cd C:\Users\bj284\Downloads\TraeTesting\cursor\Development\driver-lab\vm
.\host-guest-exec.ps1 -VmName AI-Workspace -Command powershell,-NoProfile,-ExecutionPolicy,Bypass,-File,D:\hostdev\driver-lab\vm\guest-probe-rea-env.ps1
```

## Sam — exact clicks if share missing

1. VirtualBox Manager → **AI-Workspace** → **Settings** → **Shared Folders**.
2. Add: path `C:\Users\bj284\Downloads\TraeTesting\cursor\Development`, name `hostdev`, mount `D:\hostdev`, **Auto-mount** + **Make Permanent**.
3. Guest: **Devices** → **Insert Guest Additions CD** → install → reboot.
4. Sign in as lab user; run install script above.