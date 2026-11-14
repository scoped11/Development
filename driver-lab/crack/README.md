# DriverLab Crack

Local pipeline to **qualify** kernel drivers (extract → static profile → VM load/open → primitive card).

## Encrypted / packed `.sys`

OEM inbox drivers are almost always **plain PE** (maybe stripped symbols). Some cheat loaders or packed drops use **high-entropy sections** or **runtime decrypt** in `DriverEntry`.

| Stage | Tool |
|-------|------|
| Installer still wrapped | `crack_extract` (7-Zip, plus unshield for InstallShield `data1.cab`, innounp for Inno) |
| On-disk PE looks encrypted | `crack_pe_unpack` (entropy, raw size 0 + large virtual size, RWX) |
| After the unpack stub runs in the VM | [lab_post_load_dump.md](../tools/drivers/lab_post_load_dump.md), then IDA on the fixed dump |

`guest-export-stripped-driver.ps1` copies the DriverStore file. That file is still packed if decryption happens in `DriverEntry`.

`crack_pe_unpack` does not brute-force keys. The lab guest has no kernel debugger wired, so the memory dump is a manual VM step, not an arsenal workflow.

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

Host IDA bridge: `driver-lab/reversal/reversal_bridge.py` or MCP `crack_reversal_route` → `ida_ticket.json` beside each card.

## CLI

```powershell
python driver-lab\crack\crack_cli.py profile ..\audit\inbox\oem-sys\x64\SIVX64.sys
```
