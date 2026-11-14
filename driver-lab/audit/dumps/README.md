# Memory dump registry

Validated post-load kernel images for the host reversal pipeline. Guest writes here; the main PC scores and rejects bad inputs.

## Layout

| Path | Purpose |
|------|---------|
| `inbox/<sha16>/<dump_id>/` | Canonical dump + `manifest.json` (schema v2) |
| `host-drop/*.json` | Host sidecars ingested by `crack_dump_sync` |
| `processed/host-drop/` | Archived sidecars after success |

## Guest (after `.writemem`)

```powershell
& 'Z:\driver-lab\vm\bg\guest-register-memory-dump.ps1' `
  -DumpPath 'C:\dumps\module.bin' `
  -DiskSysPath 'C:\DriverLab\greenfield\stage\packed.sys' `
  -CaptureMethod windbg `
  -KernelBase '0xffff...' `
  -Note 'post DriverEntry'
```

## Host kd (no hand-typed dump path)

UART is off until the VM is powered off once.

```powershell
powershell -File driver-lab\vm\host-enable-kd-uart.ps1
# elevated guest, then reboot:
# Z:\driver-lab\vm\bg\guest-enable-kd-serial.ps1
python driver-lab\reversal\kd_dump.py dump driver-lab\audit\inbox\oem-sys\x64\target.sys --wait-load
```

`kd.exe` is the installed WinDbg amd64 debugger. Receipts land in `audit/dumps/kd-sessions/`. Scripts land in `reversal/windbg/generated/`.

## Host

```powershell
python driver-lab\reversal\dump_registry.py scan-all
python driver-lab\reversal\reversal_bridge.py auto driver-lab\audit\inbox\oem-sys\x64\packed.sys --preset siv
```

Or drop a sidecar in `host-drop`:

```json
{
  "disk_sys_path": "C:\\Users\\baile\\OneDrive\\Desktop\\Development\\driver-lab\\audit\\inbox\\oem-sys\\x64\\target.sys",
  "dump_path": "C:\\dumps\\target.mem.bin",
  "capture_method": "writemem",
  "kernel_base": "0xfffff80638200000",
  "note": "IoCreateDevice break"
}
```

Then `crack_dump_sync` or `reversal_bridge.py auto --sync` (via `crack_reversal_auto`).

## Validation gates

- MZ + PE signature, minimum size
- **Rejected** if dump SHA256 equals on-disk driver (DriverStore copy trap)
- Score 0–100: PE OK, entropy drop, fixup success, hash differs from disk
- `validated` requires score ≥ 50

MCP: `crack_dump_register`, `crack_dump_sync`, `crack_dump_pick`, `crack_reversal_auto`.