# driver2 IOCTL reference (integration)

All codes use `FILE_DEVICE_UNKNOWN` base. **Session `0x870` first** on every new handle/process.

Structs match `include/betest_ioctl.h` / `sdk/driver2_api.h`.

---

## Core (every cheat uses these)

| IOCTL | Code | In/out | Purpose |
|-------|------|--------|---------|
| `IOCTL_SESSION_AUTH` | `0x870` | 8-byte challenge → 8-byte token | Gate; binds session to caller PID |
| `IOCTL_READ_PROCESS_MEM` | `0x805` | `PROC_READ_REQ` → data | Read game memory (any PID) |
| `IOCTL_WRITE_PROCESS_MEM` | `0x806` | `PROC_WRITE_REQ` + data | Write game memory |
| `IOCTL_READ_KERNEL_MEM` | `0x802` | `KREQ_READ` → data | Kernel VA read |
| `IOCTL_WRITE_KERNEL_MEM` | `0x803` | `KREQ_WRITE` + data | Kernel VA write (lab; PG risk) |
| `IOCTL_DELETE_SYMLINK` | `0x808` | — | Hide `\\.\` name (mapper optional; default **off**) |

---

## EAC (Rust / EOS) — lab / map-time

| IOCTL | Code | When fired |
|-------|------|------------|
| EAC improve | `0x844` | `--ac-replica rust` + `--eac 7f` |
| EOS encrypted pointers | `0x851` | `--encptr f` on rust / EOS profile |
| EAC/BE max bundle | `0x869` | `--driver2` bypass sweep |
| Disable EAC callbacks | `0x80A` | Opt-in / full modes |

**Capture:** `driver2/dumps/rust-eac-eos_*` — `EasyAntiCheat_EOS.sys` SHA in `manifest.json` (lab mirror: `driver-lab/dumps/`).

---

## BattlEye (R6, ARK, …)

| IOCTL | Code | When fired |
|-------|------|------------|
| BE improve | `0x852` | `--ac-replica r6` or `arksa` |
| Disable BE callbacks | `0x807` | Bypass sweep / flags |
| NMI neut | `0x868` | `--driver2` bypass |

**Capture:** `driver2/dumps/r6-battleye_*` — `usermode/BEService_r6s.exe`, `battleye/game/BEClient_x64.dll` (`BEDaisy.sys` often absent on disk).

---

## ACE (Delta Force, Arena Breakout) — primary focus

| IOCTL | Code | When fired |
|-------|------|------------|
| ACE strengthen | `0x846` | `--ace 3f` + `--ac-replica deltaforce|arenabreakout` |
| ACE Delta hardening | `0x84E` | `--delta 1f` (Delta Force only) |
| ACE/VGK max | `0x86A` | `--ace-vgk` on ACE replica profiles |
| Disable ACE callbacks | `0x80B` | Bypass / `--ace` flags |

**Captures:**

- **Delta Force:** `driver2/dumps/deltaforce-ace_*/kernel/ACE-GAME.sys` (game bundle; manifest hashes vs system32).
- **Arena Breakout:** `driver2/dumps/arenabreakout-ace_*/kernel/ACE-GAME.sys`.

CLI after map (custom tools): use same codes via `DeviceIoControl` on open handle — mapper already sends improve IOCTLs on bypass map.

---

## Shared bypass sweep (`--driver2` bypass)

| IOCTL | Code | Role |
|-------|------|------|
| Strip all callbacks | `0x81B` | Preempt strip |
| Inline hook neutralize | `0x81C` | Callback patch lab |
| Kill AC timers | `0x815` | Timer cleanup |
| Kill AC threads | `0x81A` | Thread cleanup |
| Unified neutralize | `0x827` | BE/EAC/ACE scan |
| NMI neutralize | `0x868` | NMI callback list |

Full list: `byovd/docs/05_existing_bypass_catalog.md` + `BetestDriver.c` defines.

---

## Error handling (usermode)

| Symptom | Typical cause |
|---------|----------------|
| `CreateFile` fails | Not mapped; wrong device name; symlink deleted |
| Auth fails | No `0x870`; wrong PID |
| Read fail | Protected PID; invalid VA; AC still blocking |
| Map exit 2 | Tier 0 — AC up, vehicle blocked |

Use `Driver2OpenWithRetry()` after map.
