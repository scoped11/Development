# crack_qualify_phys

MCP: `driverlab-crack` → `crack_qualify_phys`

Guarded SIV workflow. It does not load the mapper and it does not write physical memory.

1. Profile `SIVX64.sys` and write the crack card.
2. Guest qualify: open `\\.\Global\SIVDRIVER` (load only if it is not already open).
3. Guest `guest-siv-phys-smoke.ps1`: 16-byte read, IOCTL `0x10`, physical address `0x1000`.
4. Merge `vm_phys` onto `audit/crack/<sha16>/card.json`.

IOCTL `0x14` is sent with a 16-byte buffer only to record the expected reject. That is not a write.

```powershell
python driver-lab\arsenal\arsenal_run.py qualify_phys --preset siv
```

Any preset other than `siv` is refused before a guest job is queued.
