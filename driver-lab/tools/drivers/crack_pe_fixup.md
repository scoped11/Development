# crack_pe_fixup

MCP: `driverlab-crack` → `crack_pe_fixup`

Sets each PE section’s on-disk offset and size to match its virtual layout (same as PE-bear for `.writemem` dumps). Run on the **host** before IDA.

```powershell
python driver-lab\reversal\reversal_bridge.py fixup C:\dumps\module.bin
python -c "from pathlib import Path; import sys; sys.path.insert(0, r'driver-lab/crack'); from crack_lib import pe_fixup_mapped_for_ida; ..."
```

Output defaults to `*.ida-fixed.sys` beside the source file.