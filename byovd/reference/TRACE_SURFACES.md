# Kernel trace surfaces (kdmapper literature vs this kit)

Quick index for lab work. See `docs/15_BYOVD_AND_DRIVER_TRUST.md` and upstream `kdmapper/` for PiDDB / hash-bucket implementations.

| Artifact | kdmapper-style | `mapper_nal` (this kit) |
|---|---|---|
| PiDDBCacheTable | AVL delete | `vehicle_hygiene.hpp` (before DriverEntry; build-specific patterns) |
| g_KernelHashBucketList | Scrub | `vehicle_hygiene.hpp` |
| WdFilter runtime lists | Scrub | `vehicle_hygiene.hpp` |
| MmUnloadedDrivers | `BaseDllName.Length = 0` on vehicle | Name zeroing in hygiene (vehicle stays loaded) |
| Payload | Pool + optional stomp | Pool `ManualMap` on BYOVD primitive |
| Communication | Varies | Betest IOCTL device |

Vehicle drivers live in `vehicles/`. Reference mapper in `reference/kdmapper/`.
