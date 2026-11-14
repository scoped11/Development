# VM lab MCP (optional)

Host-side bridge into `DriverLab-E-26200` using VirtualBox **Guest Control** (not network RDP).

## Prereqs

- Guest Additions installed (unattended install does this)
- `driver-lab/vm/lab.credentials.json` (auto-created by `wait-and-bootstrap-guest.ps1`)

## Auto setup (recommended)

```powershell
cd driver-lab\vm
.\wait-and-bootstrap-guest.ps1
```

Enables VRDE :3389, NAT `127.0.0.1:2222→22`, runs `setup-guest-remote.ps1` in guest.

## Cursor MCP

```json
{
  "mcpServers": {
    "driverlab-vm": {
      "command": "python",
      "args": [
        "C:\\Users\\baile\\OneDrive\\Desktop\\Development\\driver-lab\\vm\\mcp\\vm_lab_mcp.py"
      ]
    }
  }
}
```

Install once: `pip install mcp`

**Tools:** `vm_hostname`, `vm_exec`, `vm_ensure_admin`, `vm_exec_admin`, `vm_probe_vehicles`, `vm_driver3_smoke`, `vm_driver3_lane_gate`, `vm_bootstrap`

Headless admin (no RDP): `.\ensure-guest-admin.ps1` then `.\run-vm-vehicle-probe-from-host.ps1`. Kernel map still **VM-only**.

Installed in **`~/.cursor/mcp.json`** as `driverlab-vm`. After edits: **Cursor Settings → MCP → refresh** (or reload window).

Guest share on this host is usually **`Z:\`** (repo at `Z:\byovd`, `Z:\driver-lab`).

Without MCP, use `.\host-guest-exec.ps1 -Command @('powershell.exe','-NoProfile','-Command','hostname')`.

Kernel map still needs **elevated PowerShell in the guest** (`host-b -Execute`); MCP uses non-admin Guest Control.
