# Driver2 VM — zero guesswork setup

## I run everything from the host (you)

```powershell
cd C:\Development\byovd
.\driver2\vm-lab.ps1 -Profile rust
```

When that works, continue:

```powershell
.\driver2\vm-lab.ps1 -Profile rust -Execute -ForceMap -SkipProve
```

If you see **exit 24** (worker missing), do the **one-time** step below, then run the `-Execute` line again.

---

## One-time: register elevated worker (inside VM)

1. Open VM: **Win+R** → `mstsc /v:127.0.0.1:3389` → connect as **DriverLab** / `LabVM-ChangeMe!`
2. In the VM: **Start** → type **PowerShell** → **Run as administrator**
3. Paste:

```powershell
$env:DRIVER2_VM_LAB_GUEST = '1'
$env:DRIVER2_VM_REGISTER_TASK = '1'
Set-Location Z:\byovd\driver2
powershell -ExecutionPolicy Bypass -File .\vm-lab.ps1
```

4. You should see `[+] registered Driver2VmLabWorker`
5. Close RDP. On the **host**, run:

```powershell
.\driver2\vm-lab.ps1 -Profile rust -Execute -ForceMap -SkipProve
```

Success = `vm_lab_result.json` with `"ok": true`, `probe_exit": 0`.

### Find a working vehicle (VM only)

After the worker is registered:

```powershell
# Host
cd C:\Development\driver-lab\vm
.\run-vm-vehicle-probe-from-host.ps1
```

Reads `byovd\driver2\vm_vehicle_probe.json` — tests `ts`, `corsair`, `rtcore`, `siv`, `nal` with `RwTest.sys` (`--no-fallback`).

Extract fresh ThrottleStop.sys in the guest:

```powershell
Z:\byovd\vehicles\extract-throttlestop-from-exe.ps1
```

### Stomp dry-run (image-backed payload, DriverLab-E)

Docs: **`byovd/docs/STOMP_BYPASS.md`**. Worker helpers: **`driver-lab/vm/bg/README.md`**.

```powershell
# Host — timed dry-run on guest (kills stuck mapper first)
cd C:\Development\driver-lab\vm\bg
.\host-bg-admin.ps1 -Action exec -PowerShell "& 'Z:\driver-lab\vm\bg\guest-stomp-dryrun-watchdog.ps1'; exit `$LASTEXITCODE" -TimeoutSec 140
```

Guest (admin) direct:

```powershell
& 'Z:\byovd\driver2\bin\mapper_nal.exe' 'Z:\byovd\driver2\bin\Driver2Stomp.sys' --stomp --stomp-dry-run --victim luafv --driver myport --no-auth --quiet --force-map --no-fallback
```

After host rebuild, sync `mapper_nal.exe` to the guest share before trusting dry-run speed (early stomp skips cave/NX scan).
