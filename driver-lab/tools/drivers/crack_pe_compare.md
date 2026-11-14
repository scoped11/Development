# crack_pe_compare

MCP: `driverlab-crack` → `crack_pe_compare`

Per-section Shannon entropy on **on-disk** vs **post-load dump**. `runtime_decrypt_likely` is true when high-entropy disk sections collapse in the dump.

```powershell
python driver-lab\reversal\reversal_bridge.py compare disk.sys dump.bin
```

If entropy does not change, the “dump” is probably still a DriverStore copy, not memory after `DriverEntry`.