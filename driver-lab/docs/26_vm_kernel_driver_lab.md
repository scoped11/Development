# VM lab = live EAC/BE parity (not a toy driver)

**VmLab** (`vm/driver/VmLabDrv.c`) is optional hello-IOCTL practice.  
**The real lab** is **`byovd/bin/Betest.sys`** + **`mapper_nal`** with the **same flag families** you would use under Rust (EAC) or R6 (BE) — on a **snapshot VM only**.

| Live concern | VM lab vehicle | Script / flags |
|--------------|----------------|----------------|
| Process R/W IOCTL | Betest / RwTest | `0x805` / `0x806`, session `0x870` |
| EAC family | `--ac-replica rust` | `map-ac-replica-lab.ps1` |
| BE family | `--ac-replica r6` | same |
| Preempt / strip / timers / unified neut | **`--ac-replica-bypass`** | `Mode bypass` |
| IOCTL matrix without full preempt | **`--ac-replica-ioctl`** | `Mode ioctl` |
| Hygiene + rw baseline | `--ac-safe` + RwTest | `Mode prove` |
| Catalog of every opt-in flag | | `byovd/docs/05_existing_bypass_catalog.md` |

**Host A (ranked):** Lane D + later **dispatch hijack** — do **not** map Betest there.  
**DriverLab VM:** map Betest here so detection + bypass **IOCTL behavior** matches what you will eventually need beside a primitive.

---

## Automated (guest Admin)

After `D:\hostdev` is mounted:

```powershell
# Full bypass map path (default parity)
D:\hostdev\driver-lab\vm\setup-vm-lab-betest.ps1 -Mode bypass -Profile rust

# R6 / BattlEye-shaped replica
D:\hostdev\driver-lab\vm\setup-vm-lab-betest.ps1 -Mode bypass -Profile r6

# Step-down: prove → ioctl → bypass (docs/06_better_research_method.md)
D:\hostdev\driver-lab\vm\setup-vm-lab-betest.ps1 -Mode prove
D:\hostdev\driver-lab\vm\setup-vm-lab-betest.ps1 -Mode ioctl -Profile rust
```

Replica order for **bypass** mode (see `15_test_ac_replicas.md`):

1. Snapshot `clean`
2. Map **before** starting replica AC (or `-ForceMap`)
3. Start AC **replica / harness** only — not production ranked client unless you accept ban risk
4. `D:\hostdev\byovd\bin\process_rw.exe` must still read marker

---

## Optional VmLab (teaching / service-load)

```powershell
D:\hostdev\driver-lab\vm\install-vm-lab-driver.ps1   # \\.\VmLab0 only — no AC IOCTLs
```

Does **not** replace Betest for parity testing.

---

## GH photos / advanced track (same VM, separate steps)

| Topic | Where in repo | Notes |
|-------|---------------|-------|
| IOCTL comm | Betest + `betest_ioctl.h` | Primary |
| kdmapper hygiene | mapper + `vehicle_hygiene.hpp` | VM BYOVD only |
| Dispatch / XFG hijack | `22_live_hijack_method.md` | **H2** on resident driver — not DriverEntry of Betest |
| .data / win32k hook | Research only | Not automated |
| Module stomp | `byovd/docs/STOMP_BYPASS.md` + `vm/bg/guest-stomp-dryrun-watchdog.ps1` | In-tree; guest dry-run |

**Lane E acceptance:** treat **VM bypass map + process_rw under replica** as the BYOVD-lab cousin of **H3** for ioctl path; **hijack H2** remains a separate experiment on a non-AC host driver.

---

## Build on host (shared folder)

```powershell
cd byovd
.\build.ps1
```

Guest scripts pick up `D:\hostdev\byovd\bin\*` automatically.
