# REA on the lab VM

Workflow skill (host copy): `Reversing\.cursor\skills\rea-ai-reversing\SKILL.md` and Project store `workflows/rea-ai-reversing/SKILL.md`.

## Scope

- Static PE/kernel **documentation** on lab drivers (P_Finger_ByPass kit, BYOVD samples on the share).
- **No** live anti-cheat evasion, **no** copying production EAC/BE dumps from game installs.

## Guest setup checklist

1. Toolchain per [vm-artex-toolchain.md](vm-artex-toolchain.md) (`GHIDRA_INSTALL_DIR=C:\lab\tools\ghidra\ghidra_12.1.4_PUBLIC`).
2. `npx.cmd -y rea-agents@latest doctor` → ghidra **healthy**.
3. Cursor MCP: `rea` + optional `ghidra` bridge in `.cursor\mcp.json`.
4. Open project from `Z:\` (when share mounted) or `C:\lab\P_Finger_ByPass`.

## Agent workflow (REA)

1. **Scope** in chat: lab CTF, owned repo binaries only.
2. `open_binary` on `C:\DriverLab\*.sys` or `kit\*.sys` (after staging to local disk).
3. PE tools without Ghidra: `inspect_pe_imports`, `inspect_pe_certificates`, `inspect_pe_load_config`.
4. With Ghidra healthy: `binary_overview`, dispatch/IOCTL tracing, pseudocode — cite addresses in `artifacts/` or `docs/re-notes-*.md`.
5. Kernel load failures: run `scripts\Vm-Lab-DiagnoseDriverLoad.ps1` on guest (elevated); correlate with SCM exit codes (2 = path/share, 577 = CI unsigned).

## Host guestcontrol

`host-guest-exec.ps1` resolves VM from `lab.credentials.json` and uses full `WindowsPowerShell\v1.0\powershell.exe` (fresh Windows images lack `System32\powershell.exe` shim).

```powershell
.\host-guest-exec.ps1 -TimeoutMs 600000 -Command @(
  'powershell','-NoProfile','-ExecutionPolicy','Bypass',
  '-Command','npx.cmd -y rea-agents@latest doctor'
)
```

## Snapshots

| File | Meaning |
|------|---------|
| `docs\rea-doctor-guest-snapshot.txt` on share | Latest guest doctor (overwritten after install) |
| `C:\lab\logs\rea-doctor-guest-snapshot.txt` | Guest-local copy |

Expected: `ghidra` healthy; `environment_healthy` may stay false without Hopper/IDA — see [rea-doctor-guest-expected.md](rea-doctor-guest-expected.md).