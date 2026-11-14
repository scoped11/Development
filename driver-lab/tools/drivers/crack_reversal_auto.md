# crack_reversal_auto

MCP: `driverlab-crack` → `crack_reversal_auto`

One-shot host pipeline:

1. `crack_dump_sync` — validate guest manifests + host-drop sidecars  
2. `crack_dump_pick` — best scored dump if any  
3. `crack_reversal_route` — ticket + IDA path (fixup dump when picked)  
4. `crack_qualify_vm` when PE still packed and no valid dump  

```powershell
python driver-lab\reversal\reversal_bridge.py auto driver-lab\audit\inbox\oem-sys\x64\target.sys --preset siv
```

Response includes `awaiting_dump` when no validated dump exists yet.