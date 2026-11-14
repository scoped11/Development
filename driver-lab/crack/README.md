# DriverLab Crack

Local pipeline to **qualify** kernel drivers (extract → static profile → VM load/open → primitive card).

## Anti-cheat

**This stack does not bypass EAC, BattlEye, ACE, Vanguard, or any paid title protection.**

- `lane_assess` / `crack_lane_matrix` only report **local** CI blocklist + heuristic name matches.
- Public LOLDrivers hashes and classic mapper vehicle names are flagged `avoid_mapper_vehicle`.
- Unknown inbox hashes still need VM IOCTL proof; that is **lab research**, not live readiness.
- For title routing use `byovd/driver3/lane-gate.ps1` and `Test-LiveReady.ps1` — not crack cards.

## MCP

Register `driverlab-crack` in `mcp.json`, restart MCP, then:

- `crack_profile` on a `.sys`
- `crack_lane_matrix` before planning any vehicle
- `crack_qualify_vm` for elevated load/open smoke

Cards land in `driver-lab/audit/crack/<sha16>/card.json`.

## CLI

```powershell
python driver-lab\crack\crack_cli.py profile ..\audit\inbox\oem-sys\x64\SIVX64.sys
```
