# Scale A — cheat-dev SDK target (80–90%)

**Scale A** = split-trust product quality: **not** undetected live ranked.

| Bar | Artifact |
|-----|-----------|
| Gate | `lane-gate.ps1` / `gate-all.ps1` |
| build_id | `Test-Driver3BuildId.ps1` + `lab_manifest.json` |
| Host A discipline | `host-a.ps1` (DMA only; build_id gate on `-Execute`) |
| Host B proof | `host-b.ps1` / VM elevated worker |
| Ranked fail-closed | `driver2` refuses ranked map without override |
| Scoreboard | `build-dashboard.ps1` → `dashboard.json` |

## VM elevated runner (one-time)

In **elevated** guest PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File Z:\byovd\driver3\register-driver3-elevated-task.ps1
```

Then from host:

```powershell
.\driver3\vm-test-from-host.ps1 -Profile rust -Execute -ForceMap -SkipProve
```

## One command (host)

```powershell
.\driver3\reach-scale-a.ps1 -AllTitles          # static + manifest + VM smoke → ~80%+
.\driver3\reach-scale-a.ps1 -AllTitles -VmMap  # + Host B map + guest log sync → 90%+

**Developer respect (separate from “undetected”):** `developer-nod.ps1 -AllTitles` → `developer_nod.json` (target ≥80%). Live play still needs `Test-LiveReady.ps1` before `ranked.ps1 -Execute`. See [`RESPECT.md`](RESPECT.md).
```

## Dashboard

```powershell
.\driver3\build-dashboard.ps1 -AllTitles
```

Target **score_pct ≥ 80** before calling Scale A “shippable SDK.”
