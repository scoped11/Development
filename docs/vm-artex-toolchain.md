# VM ARTEX + RE toolchain (AI_Workspave_v3)

Lab placement: **guest** installs; **host** drives `VBoxManage guestcontrol` and holds the Development share source.

## Paths (guest)

| Component | Path |
|-----------|------|
| Lab root | `C:\lab` |
| Ghidra 12.1.4 (REA) | `C:\lab\tools\ghidra\ghidra_12.1.4_PUBLIC` (`GHIDRA_INSTALL_DIR`) |
| ARTEX v0.3.30 | `C:\lab\tools\artex\artex-0.3.30-windows-amd64` |
| Reference clones | `C:\lab\tools\repos\` |
| Install log | `C:\lab\logs\install-toolchain.log` |
| Doctor snapshot | `C:\lab\logs\rea-doctor-guest-snapshot.txt` (copied to share when mounted) |

Ghidra zip is the official **12.1** release asset; the folder is renamed to `ghidra_12.1.4_PUBLIC` so REA/GhidraMCP docs match.

## Host → guest

| Item | Value |
|------|--------|
| VM name | `AI_Workspave_v3` (`lab.credentials.json`) |
| Share source | `C:\Users\bj284\Downloads\TraeTesting\cursor\Development` |
| Guest mount | **`Z:\`** (`\\VBoxSvr\hostdev`) — **not** bare `D:\` (GA ISO) |
| Guestcontrol | **`\\VBoxSvr\hostdev`** — mapped `Z:\` is **not** visible in VBox guestcontrol sessions (Sam’s interactive `Z:\` is fine) |

If `net use Z: \\VBoxSvr\hostdev` fails: close the VirtualBox GUI lock, then on the host:

```powershell
cd C:\Users\bj284\Downloads\TraeTesting\cursor\Development\driver-lab\vm
.\wire-ai-workspace-lab.ps1 -VmName AI_Workspave_v3
```

## Install (idempotent)

**Host:**

```powershell
cd C:\Users\bj284\Downloads\TraeTesting\cursor\Development\driver-lab\vm
.\host-run-toolchain-install.ps1 -SkipHeavyWinget   # faster REA/Ghidra pass; runs UNC installer on guest
# full winget (WinDbg, Docker, WDK): omit -SkipHeavyWinget
```

**UAC note:** winget often returns **1602** under guestcontrol (no UAC). The installer falls back to portable **Node** + **JDK** under `C:\lab\tools\` and continues Ghidra/REA without elevation.

**Guest (manual, elevated):**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\lab\bootstrap\install-vm-artex-toolchain.ps1
```

Re-run is safe: existing Node/JDK/Ghidra/repos are skipped.

## REA + MCP

Prerequisites: Node 22+, JDK 21, `GHIDRA_INSTALL_DIR` as above.

```powershell
npx.cmd -y rea-agents@latest doctor
npx.cmd -y rea-agents@latest setup    # once, select Cursor
```

Guest `%USERPROFILE%\.cursor\mcp.json` and `C:\lab\.cursor\mcp.json` register:

- **`rea`** — `npx.cmd rea-agents@latest mcp` with `JAVA_HOME` / `GHIDRA_INSTALL_DIR`
- **`ghidra`** — `ghidra-mcp --ghidra-server http://127.0.0.1:8080/` (after `pip` install from LaurieWired/GhidraMCP)

Install the GhidraMCP extension from the GitHub release zip in Ghidra **File → Install Extensions**, enable **GhidraMCPPlugin**, start the HTTP server (default port 8080).

See also: [rea-vm.md](rea-vm.md).

## Smoke

```powershell
$env:Path = 'C:\lab\tools\node;' + $env:Path
$env:GHIDRA_INSTALL_DIR = 'C:\lab\tools\ghidra\ghidra_12.1.4_PUBLIC'
$env:JAVA_HOME = 'C:\lab\tools\jdk-21'
C:\lab\tools\node\npx.cmd -y rea-agents@latest doctor
# or after a partial install:
powershell -NoProfile -ExecutionPolicy Bypass -File \\VBoxSvr\hostdev\driver-lab\vm\finish-rea-guest.ps1
```

**Doctor “healthy” for this lab:** `node` **26.13.1+** (not 22.19 portable default), all **`ghidra-*` checks healthy**, `analyzeHeadless` OK. Top-level `healthy: false` is **expected** (no Hopper/IDA, `rea setup` not run in guest Cursor).

**Node:** portable default is **v26.13.1** (`upgrade-node-guest.ps1` if an older portable was installed).

## Driver SCM lab (P_Finger_ByPass)

```powershell
# guest interactive Admin, or host:
.\host-run-diagnose-elevated.ps1   # needs schtasks HIGHEST (may fail from guestcontrol)
.\scripts\Vm-Lab-DiagnoseDriverLoad.ps1
```

**guestcontrol is not elevated** → `sc create` fails (OpenSCManager 5). Staging + `NotSigned` on `payload.sys` still runs. **577 control** needs one elevated guest run; repo A3 documents 577 without swap.

## WDK status

`Microsoft.WindowsWDK.10.0.26100` is attempted at end of install (unless `-SkipWdk`). Log: `C:\lab\logs\wdk-winget.log`. Often requires **Visual Studio C++ desktop workload** first — document outcome there; no DSE-disable steps in this lab.

## P_Finger_ByPass driver lab

CTF diagnosis only: `P_Finger_ByPass\scripts\Vm-Lab-DiagnoseDriverLoad.ps1` (stage to `C:\DriverLab`, compare share vs local `sc start`). Root causes on fresh VMs: **no hostdev share**, **loading `.sys` from share (exit 2)**, **unsigned payload 577 without swap**, **Defender quarantine**.

REA static notes (host PE tools, kit drivers):

| File | SHA256 (short) | Cert table |
|------|----------------|------------|
| `kit\MsIo64.sys` | `a7261f13…` | WHCP signed (vehicle) |
| `kit\payload.sys` | `3bc356c9…` | **absent** (unsigned; CI 577 without swap) |

## Related

- [rea-vm.md](rea-vm.md)
- [vm-checklist.md](vm-checklist.md)
- [rea-doctor-guest-expected.md](rea-doctor-guest-expected.md)