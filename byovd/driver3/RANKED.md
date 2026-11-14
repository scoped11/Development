# driver3 ranked mode

**Ranked** = you are on the **game PC** with live EAC/BE/ACE. driver3 **only** runs **Host A**.

| Do on ranked PC | Do on DriverLab VM |
|-----------------|-------------------|
| `ranked.ps1 -Execute` | `host-b.ps1 -Execute -ForceMap` |
| DMA / `dma/` overlay | driver2 map + IOCTL proof |
| `sync-build-id-from-dumps.ps1` | `gate-all`, D3i optional |

## Commands

```powershell
cd byovd

# Preflight (plan)
.\driver3\ranked.ps1 -Game rust -PreflightOnly -SyncBuildId

# Ranked session (build DMA + manifest)
.\driver3\ranked.ps1 -Game rust -Execute -SyncBuildId

# Shortcut from repo root
.\ranked.ps1 -Game rust -Execute
```

## Artifacts

- `ranked_session.json` — last ranked preflight/session
- `docs/lab_manifest.json` → `lane_d.*` synced with VM

## Not ranked

- `driver2\everything.ps1 -Target ranked -Execute` — **blocked** (use `DRIVER2_RANKED_OVERRIDE=1` only for lab exceptions)
- Kernel map on game PC — **Tier 0**; see `drivers/route-dma-host-a.md`

## Respect bar (developer nod)

```powershell
.\driver3\developer-nod.ps1 -AllTitles    # → developer_nod.json
.\driver3\Test-LiveReady.ps1 -Game rust  # required before -Execute
.\driver3\ranked-ci.ps1 -AllTitles
```

See `RESPECT.md`. **Not claimed:** undetected, D4, DMA hardware autodetect.
