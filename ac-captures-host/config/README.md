# Lab config

| File | Use |
|------|-----|
| `ac_host_captures.json` | SHA256, SGuard builds, dump paths — ties captures to static analysis |
| `ac_replica_profiles.json` | `prove` / `ioctl` / `bypass` flag lists for `map-ac-replica-lab.ps1` and `mapper_nal --ac-replica` |

Mapper reads profiles from `byovd\docs\` during scripts; this folder is the **bundle copy** kept in sync with that tree.

See `..\docs\DRIVER_LAB_GUIDE.md` §3 for profile ↔ game mapping.
