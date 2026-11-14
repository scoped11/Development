# crack_reversal_route

MCP: `driverlab-crack` → `crack_reversal_route`

Host-only bridge between crack cards and `user-ida-pro-mcp`.

1. `crack_profile` + `crack_pe_unpack` verdict
2. Copy into `audit/inbox/ida-staging/<sha16>/`
3. Write `audit/crack/<sha16>/ida_ticket.json` with `ida_input_path`, `route`, and MCP tool sequence

| Route | Meaning |
|-------|---------|
| `static_ida` | Open staged `.sys` on main PC now |
| `vm_qualify_then_dump` | Packed — qualify in VM, kernel dump, then re-run with `memory_dump` |
| `fix_dump_then_ida` | You passed a `.writemem` file; optional auto fixup → IDA |

Related tools: `crack_pe_fixup`, `crack_pe_compare`.

Prefer one shot: [`crack_reversal_auto.md`](crack_reversal_auto.md).

```powershell
python driver-lab\reversal\reversal_bridge.py route driver-lab\audit\inbox\oem-sys\x64\SIVX64.sys
python driver-lab\reversal\reversal_bridge.py route packed.sys --sync-registry --auto-dump
python driver-lab\reversal\reversal_bridge.py route SIVX64.sys --qualify --preset siv
python driver-lab\reversal\reversal_bridge.py route disk.sys --dump C:\dumps\unpacked.bin
```

Then in Cursor:

```text
idb_open input_path=<ida_input_path from ticket> mode=prefer_headless
```

Full architecture: [reversal/README.md](../../reversal/README.md).