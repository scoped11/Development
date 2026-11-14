# DriverLab Research Hub (MCP)

Local **research + hash identity** server for BYOVD / OEM driver work. Connect from Cursor as MCP server `driverlab-research`.

## What it does

- Search your `driver-lab/audit/*.md` notes
- SHA256 files and match **LOLDrivers** / policy JSON (on disk)
- Fingerprint installers (PE, NSIS, InstallShield, fake HTML downloads)
- Queue the same elevated VM probes as `driverlab-vm` (`siv_phys`, `greenfield`, etc.)
- Read `bg/out` job JSON and greenfield reports

## What it deliberately does **not** do

- No MD5/password “cracker” websites or rainbow tables
- No phishing, DDoS, AD exploit, or mobile malware tooling
- No network scanning of third-party targets

For reversing, keep using **IDA MCP** and **driverlab-vm** alongside this hub.

## Cursor setup

Add to `.cursor/mcp.json`:

```json
"driverlab-research": {
  "command": "C:\\Users\\baile\\AppData\\Local\\Programs\\Python\\Python311\\python.exe",
  "args": ["C:\\Users\\baile\\OneDrive\\Desktop\\Development\\driver-lab\\research-hub\\hub_mcp.py"],
  "cwd": "C:\\Users\\baile\\OneDrive\\Desktop\\Development\\driver-lab\\research-hub",
  "env": {
    "DRIVERLAB_VM": "DriverLab-E-26200",
    "DRIVERLAB_REPO": "C:\\Users\\baile\\OneDrive\\Desktop\\Development"
  }
}
```

Restart MCP. Try: `hub_catalog`, `hub_hash_lookup` on `SIVX64.sys`, `hub_oem_run` preset `siv_phys`.
