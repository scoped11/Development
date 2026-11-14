# Dump registry (crack_dump_*)

MCP tools on `driverlab-crack` (also `crack_kd_dump`):

| Tool | Action |
|------|--------|
| `crack_dump_sync` | `scan-all` — guest inbox + host-drop |
| `crack_dump_register` | Validate score ingest one dump |
| `crack_dump_pick` | Highest-scoring validated dump for a `.sys` |

CLI:

```powershell
python driver-lab\reversal\dump_registry.py register disk.sys C:\dumps\mem.bin
python driver-lab\reversal\dump_registry.py pick disk.sys
python driver-lab\reversal\dump_registry.py scan-all
```

Guest: `vm/bg/guest-register-memory-dump.ps1`  
Storage: `driver-lab/audit/dumps/` — see [README](../../audit/dumps/README.md).

**Rejected automatically:** dump byte-identical to on-disk file (common DriverStore mistake).