# Expected `rea doctor` output — AI-Workspace guest

Run on the **guest** after [bootstrap-vm-tools.ps1](../driver-lab/vm/bootstrap-vm-tools.ps1) or [install-ghidra-rea-guest.ps1](../driver-lab/vm/install-ghidra-rea-guest.ps1):

```powershell
npx.cmd -y rea-agents@latest doctor | Tee-Object D:\hostdev\docs\rea-doctor-guest-snapshot.txt
# optional JSON: guest-probe-rea-env.ps1 writes docs\rea-doctor-guest-snapshot.json
```

## Target state (healthy pipeline)

After Ghidra 12.x is extracted and `GHIDRA_INSTALL_DIR` is set in `%USERPROFILE%\.cursor\mcp.json`, doctor should report **healthy: true** (or all critical scope checks passing). Shape follows REA CLI **6.9.x** (host reference 2026-11-17):

```json
{
  "healthy": true,
  "identity": {
    "cli_package_version": "6.9.0"
  },
  "scope_checks_summary": {
    "node": {
      "ok": true,
      "detail": "22.19+ or 24.x LTS"
    },
    "host": {
      "ok": true,
      "detail": "win32 x64"
    },
    "ghidra": {
      "ok": true,
      "detail": "GHIDRA_INSTALL_DIR=C:\\Tools\\ghidra_12.1_PUBLIC"
    },
    "registration:cursor": {
      "ok": true,
      "detail": "rea MCP registered with JAVA_HOME + GHIDRA_INSTALL_DIR"
    }
  },
  "providerInspections": {
    "ghidra": {
      "checks": {
        "configuration": { "ok": true },
        "launch": { "ok": true }
      }
    }
  }
}
```

## Acceptable interim (pre-setup)

| Check | Expected before `npx rea-agents setup` |
|-------|------------------------------------------|
| `node` | `ok: true` |
| `ghidra` | `ok: true` after install script |
| `registration:cursor` | May be `false` until interactive `setup` selects Cursor |

## Failure patterns (fix on guest)

| `scope_checks_summary` | Remediation |
|------------------------|-------------|
| `ghidra` → `GHIDRA_INSTALL_DIR is not set` | Re-run install script; set env in `.cursor\mcp.json` per [mcp-rea-guest.example.json](mcp-rea-guest.example.json) |
| `registration:cursor` → foreign ownership | Merge `rea` block from example JSON; restart Cursor |
| `node` → fail | `winget install OpenJS.NodeJS.LTS` (install script does this) |

## Host vs guest

| Machine | Ghidra in doctor | Notes |
|---------|------------------|-------|
| Host / cloud agent | **Not required** | REA may open PE; `binary_overview` needs guest Ghidra |
| Guest `AI-Workspace` | **Required** | Source of truth for static RE |

Compare live output to [rea-vm-smoke-test.md](rea-vm-smoke-test.md) after each install or MCP change.