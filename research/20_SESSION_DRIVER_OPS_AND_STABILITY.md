# Driver operations, hooks, and stability (session record)

**Edition:** 13 November 2026 | Canonical user-facing guide: `ac-captures-host/docs/DRIVER_LAB_GUIDE.md`.

## 1. Stack location

| Piece | Path |
|-------|------|
| Build | `byovd/build.ps1` → `bin/mapper_nal.exe`, `Betest.sys`, `RwTest.sys`, `process_rw.exe` |
| IOCTL contract | `byovd/include/betest_ioctl.h` (copy in `ac-captures-host/sdk/`) |
| Example client | `byovd/examples/process_rw.cpp` |

## 2. Map modes (BSOD risk ordering)

| Mode | Command pattern | Risk |
|------|-----------------|------|
| **Prove R/W** | `RwTest.sys` + `--ac-safe` + `--ac-replica <title>` | **Low** — rw-only, no AC IOCTL spray |
| **Title IOCTL** | `Betest.sys` + `--ac-replica-ioctl` | **Medium** — title masks only if preempt block skipped |
| **Bypass harness** | `--ac-replica-bypass` | **High** — strip, NMI `0x868`, `0x869`, `0x86A` (VM snapshot) |
| **`--ac-preempt` / `--full`** | Manual | **Very high** / PatchGuard |

Default Betest map: **`g_rwOnly = true`** — mapper does not auto-fire strip/kill/DKOM/mutate (`byovd/docs/02_architecture.md`).

**Vehicle rule:** Do not `sc stop` vehicle after map — UAF BSOD. Reboot to unload.

## 3. Client workflow (read/write)

1. `BetestOpenDevice()` — dynamic `\\.\MsFlt0` … names
2. `IOCTL_SESSION_AUTH` (`0x870`)
3. `IOCTL_READ_PROCESS_MEM` / `WRITE` (`0x805` / `0x806`) — preferred for game VA
4. Kernel `0x802–0x804` — lab only, high BSOD risk

Marker test: `0x1122334455667788` via `process_rw.exe` after prove map.

## 4. “Hooking” taxonomy (research framing)

| Layer | Mechanism | Where documented |
|-------|-----------|------------------|
| Usermode → payload | `DeviceIoControl` on mapped device | `byovd/docs/01_read_write.md` |
| AC callback patch | Betest `0x81B` strip, `0x81C` inline hook | `05_existing_bypass_catalog.md` |
| Map-time vehicle wrap | HAL tramp + real `DRIVER_OBJECT` | `02_architecture.md` |
| Live ranked host | Hijack **non-AC** dispatch (Lane E) | `driver-lab/docs/22_live_hijack_method.md` |

**Stomp:** lab path in tree (`byovd/docs/STOMP_BYPASS.md`). **Not in kit:** PiDDB scrub, IOCTL-only hijack without engineering (see catalog § “What is not”).

## 5. AC profile quick reference

| `--ac-replica` | Primary Betest IOCTLs | Capture dump |
|----------------|----------------------|--------------|
| rust | EAC improve / `0x851` | `rust-eac-eos_*` |
| r6, arksa | BE `0x852` | `r6-battleye_*` |
| deltaforce | `0x846` + `0x84E` | `deltaforce-ace_*` (use **game** ACE-GAME hash) |
| arenabreakout | `0x846` | `arenabreakout-ace_*` |

## 6. Stability work noted (session)

| Issue | Mitigation applied / planned |
|-------|---------------------------|
| OneDrive dump hang | Stage `D:\driver-lab-ac-dumps`, mirror to `driver-lab/dumps` |
| Full Betest preempt on game PC | Use prove + Lane E; bypass only on VM |
| `g_nmiNeutralize` on replica ioctl | Documented as PG-sensitive; prefer prove path on host |
| Delta ACE-GAME ≠ system32 | Profiles + captures point IDA at game bundle |

User asked to **reduce BSOD when running driver** — operational guidance: default to **RwTest + prove** on any host you care about; reserve Betest bypass for **reverted VM**; never `--full` on build 26200 game PC.

## 7. HijackDriver.exe (VM one-shot)

| Item | Path |
|------|------|
| Guide | `driver-lab/vm/hijack-driver/README.md` |
| Source | `driver-lab/vm/hijack-driver/HijackDriver.cpp` |
| Build | `driver-lab/vm/hijack-driver/build.ps1` |
| Output | `driver-lab/vm/bin/HijackDriver.exe` (requires Administrator) |
| Guest deploy/run | `driver-lab/vm/deploy-run-hijack-driver-guest.ps1` |

Flow: resolve `D:\hostdev\byovd` (or `BYOVD_ROOT`) → map `RwTest.sys` with `--ac-safe --ac-replica <profile>` → `IOCTL 0x870` + marker read `0x805`. Process **write** path: `betest_ioctl.h` / `process_rw.exe` (documented in hijack-driver README).

Guest Additions required for host `VBoxManage guestcontrol`; session **2026-11-13** guest was not GA-ready yet.

## 8. Bundle index

`ac-captures-host/` contains dumps, inbox, config JSON, SDK, examples, and `docs/DRIVER_LAB_GUIDE.md`. `MANIFEST.json` lists inbox SHA256.

## 8. Related research chapters

- [15_BYOVD_AND_DRIVER_TRUST.md](15_BYOVD_AND_DRIVER_TRUST.md)
- [16_EAC_BE_KERNEL_RESEARCH.md](16_EAC_BE_KERNEL_RESEARCH.md)
- [18_AC_REPLICA_MAPPER_INTEGRATION.md](18_AC_REPLICA_MAPPER_INTEGRATION.md)
