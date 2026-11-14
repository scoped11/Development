# What serious devs nod at (not “undetected”)

| Check | Script / artifact |
|-------|-------------------|
| Split-trust, ranked fail-closed | `ranked.ps1`, `driver2/everything.ps1` |
| Per-title EAC / BE / ACE routes | `drivers/*.md`, `games.json` |
| **Latest** dump → `build_id` | `sync-build-id-from-dumps.ps1` (glob latest folder) |
| Dump not ancient | `lib/Test-DumpFreshness.ps1` |
| Host A live bar | `Test-LiveReady.ps1` |
| VM regression smoke | `vm-test-from-host.ps1` |
| Optional VM kernel proof | `vm_hostb_execute.log` |
| One scoreboard | `developer-nod.ps1` → `developer_nod.json` |

```powershell
.\driver3\developer-nod.ps1 -AllTitles
.\driver3\ranked-ci.ps1 -AllTitles
.\driver3\Test-LiveReady.ps1 -Game rust
.\driver3\ranked.ps1 -Game r6 -PreflightOnly -SyncBuildId
```

**Still not claimed:** undetected, D4, DMA card presence, competitive retail product.

**Scale S** (above respect): [`SCALE_S.md`](SCALE_S.md) — `reach-scale-s.ps1 -AllTitles -VmMap` when VM elevated task is registered.
