# Driver tools

Lab qualification tools for inbox `.sys` files. These do not bypass anti-cheat.

## MCP (`driverlab-crack`)

Restarted with the other DriverLab servers. Tools:

| Tool | Page |
|------|------|
| `arsenal_catalog` | [arsenal_catalog.md](arsenal_catalog.md) |
| `arsenal_run` | [arsenal_run.md](arsenal_run.md) |
| `crack_pe_unpack` | [crack_pe_unpack.md](crack_pe_unpack.md) |
| `crack_reversal_route` | [crack_reversal_route.md](crack_reversal_route.md) |
| `crack_pe_fixup` | [crack_pe_fixup.md](crack_pe_fixup.md) |
| `crack_pe_compare` | [crack_pe_compare.md](crack_pe_compare.md) |
| `crack_reversal_auto` | [crack_reversal_auto.md](crack_reversal_auto.md) |
| `crack_dump_sync` | [crack_dump_registry.md](crack_dump_registry.md) |
| `crack_dump_register` | [crack_dump_registry.md](crack_dump_registry.md) |
| `crack_dump_pick` | [crack_dump_registry.md](crack_dump_registry.md) |
| `crack_profile` | [crack_profile.md](crack_profile.md) |
| `crack_lane_matrix` | [crack_lane_matrix.md](crack_lane_matrix.md) |
| `crack_qualify_vm` | [crack_qualify_vm.md](crack_qualify_vm.md) |
| `crack_qualify_phys` | [crack_qualify_phys.md](crack_qualify_phys.md) |
| `crack_extract` | [crack_extract.md](crack_extract.md) |
| `crack_find_sys` | [crack_find_sys.md](crack_find_sys.md) |
| `crack_job` | [crack_job.md](crack_job.md) |
| `crack_worker_status` | [crack_worker_status.md](crack_worker_status.md) |

Notes, not MCP tools: [lab_post_load_dump.md](lab_post_load_dump.md) for a packed `.sys` after it loads in the VM.

`driverlab-vm` also exposes `lab_crack_profile` and preset `crack_qualify_siv`.

## CLI

```powershell
python driver-lab\arsenal\arsenal_run.py list
python driver-lab\arsenal\arsenal_run.py profile --preset siv
python driver-lab\arsenal\arsenal_run.py qualify --preset siv
python driver-lab\arsenal\arsenal_run.py qualify_phys --preset siv
python driver-lab\arsenal\arsenal_run.py qualify --preset sonix
python driver-lab\arsenal\arsenal_run.py qualify --preset ioc
python driver-lab\crack\crack_cli.py profile driver-lab\audit\inbox\oem-sys\x64\SIVX64.sys
```

## Workflows

| Name | Steps |
|------|--------|
| `profile` | Static primitive card |
| `qualify` | Card, then guest open (existing device first, then load) |
| `qualify_phys` | SIV only: qualify, then read-only IOCTL `0x10` at PA `0x1000`, stored as `vm_phys` |
| `extract_profile` | Unpack installer, card each `.sys` |

Catalog: `driver-lab/arsenal/catalog.json`.

## Presets

| Preset | Driver | Device |
|--------|--------|--------|
| `siv` | `SIVX64.sys` | `\\.\Global\SIVDRIVER`, `\\.\SIVDRIVER` (SeLoadDriverPrivilege) |
| `sonix` | `SONiXDDRx64.sys` | `\\.\SONiXDDR_0_0_0_1` |
| `ioc` | `iocbios2.sys` | `\\.\iocbios2` |

Guest script: `driver-lab/vm/bg/guest-driver-crack-qualify.ps1`.

Cards: `driver-lab/audit/crack/<sha16>/card.json`.

Latest SIV open: job `78c51c5d8995`, `CREATEFILE_OK \\.\Global\SIVDRIVER`.
