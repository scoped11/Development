# DriverLab Arsenal

Workflow launcher over the crack pipeline. Same idea as a toolkit catalog, scoped to **your** drivers: profile a `.sys`, qualify open/load on the lab VM, or unpack an installer and card every extracted driver.

It does not bypass anti-cheat. `qualify` only checks `CreateFile` in the guest.

```powershell
python driver-lab\arsenal\arsenal_run.py list
python driver-lab\arsenal\arsenal_run.py profile --preset siv
python driver-lab\arsenal\arsenal_run.py qualify --preset siv
```

MCP (after restart): `arsenal_catalog`, `arsenal_run` on `driverlab-crack`.
