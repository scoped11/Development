# VM-B: hijack host shortlist (inside test guest only)

**Do not** run dispatch patches or IOCTL fuzz on the Rust gaming host. All **H2** work happens in **`DriverLab-E-26200`** first.

Refs: [`22_live_hijack_method.md`](22_live_hijack_method.md) (XFG chokepoint), [`24_vm_exploit_hunt.md`](24_vm_exploit_hunt.md).

---

## Selection rubric (no offsets in git)

| Criterion | Why |
|-----------|-----|
| `State = Running` in guest | Hijack targets **resident** drivers |
| **Not** EAC/BE/ACE/LOL | `EasyAntiCheat_*`, `BEDaisy`, iqvw/Nal/RTCore, dbk/kph |
| Microsoft-signed preferred | Stomp/cover story if you go that route later |
| ASCII strings: `__guard_dispatch_icall_fptr` + `_guard_xfg_dispatch_icall_nop` | XFG chokepoint pattern |
| User can `CreateFile("\\.\…")` | IOCTL client path exists |
| Quiet IOCTL traffic | Avoid filters/storage stack you touch in normal use |

**AC modules are for static IOCTL study**, not as the hijack host.

---

## Guest workflow

After Windows + Guest Additions + shared folder `D:\hostdev`:

```powershell
# Admin
D:\hostdev\driver-lab\vm\guest-hunt-bootstrap.ps1
D:\hostdev\driver-lab\scripts\probe-hijack-candidates.ps1 -OpenDeviceProbe
```

Outputs land in `D:\hostdev\driver-lab\audit\hijack_candidates_vm_*.csv` (visible on host).

Then on **host or guest IDA** (from shared `reverse/`):

1. Open top-scored `.sys` from CSV path.
2. Locate `_guard_xfg_dispatch_icall_nop` and `__guard_dispatch_icall_fptr` (RVAs in private notes, not repo).
3. Find user-visible symlink (`\Device\…` → `\\.\…`) via service registry / known docs.
4. VM only: prove **H2** with debugger kW or Lane-E primitive — snapshot `clean` before/after.

---

## VM install (host)

If the VDI is still empty (~2 MB):

```powershell
cd driver-lab\vm
.\install-vm-unattended.ps1
```

When install finishes, RDP (optional VRDE 3389), sign in as `DriverLab`, run bootstrap + probe above, then:

```powershell
.\host-vm-headless.ps1 -Action snapshot -SnapshotName clean
```

---

## Detection rows to log while testing (Lab I)

From public write-ups — record pass/fail on **VM-B only**:

- `MajorFunction[14]` not equal to on-disk baseline
- `__guard_dispatch_icall_fptr` differs from fresh boot
- In-memory PE vs file on disk (stomp)
- NMI / stack walk with handler PC (if you enable AC service in VM)
