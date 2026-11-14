# driver2 realtime confidence (85–90% band)

**“Realtime” here** = **replica-up lab** on DriverLab VM: map **Driver2.sys**, AC replica running (no game), **`process_rw` still works** — not production ranked bypass.

## Claimed band (85–90%)

We assign **85–90%** only when **`realtime-gate.ps1` passes** on the lab guest:

```powershell
# DriverLab guest, Admin, clean snapshot, no game clients
cd D:\hostdev\byovd\driver2
.\realtime-gate.ps1 -Profile both -ForceMap
```

Pass → `driver2/confidence.json` with `"all_pass_85": true` and per-profile `score_pct >= 85`.

### What the score measures (7 checks × profile)

| Check | Intent |
|-------|--------|
| `map_exit_0` | Mapper + Driver2 profile completed |
| `pf_diag_bypass` | Strip + title/bypass signals in log |
| `strip` / `max869` | Callback strip + EAC/BE max bundle fired |
| `process_rw` | Session + IOCTL R/W alive after map |
| `preflight_not_tier0` | Vehicle openable on this boot |
| `hygiene_log` | PiDDB/CI/WdFilter hygiene ran (trace reduction) |

**Below 85** until the gate passes on **your** VM — do not treat docs as proof.

## What driver2 adds for trace / realtime (vs plain Betest map)

| Layer | driver2 behavior |
|-------|------------------|
| Mapper | `--driver2` → hygiene + stealth `0x3f` + bypass masks + no DKOM unlink default |
| Payload | `DRIVER2_PRODUCT`: **no** DriverEntry image-load neutralize (mapper-ordered sweep) |
| Post-map | `0x808` delete user symlink (keep mapper handle; client re-opens device) |
| Runner | Always passes `--driver2` on bypass/ioctl |

**Stomp:** in-tree (`docs/STOMP_BYPASS.md`); not scored until VM S2 gates pass — default map still **pool** execution (lab risk).

**Still not implemented:** IOCTL hijack-only comm, kdmapper-full.

## Live ranked

Even with gate pass, **live Rust/R6** stays **low confidence** (vehicle block, server-side). driver2 targets **L3 replica realtime** per `docs/27_vm_bypass_confidence.md`.
