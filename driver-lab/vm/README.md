# Headless exploit lab VM

**VM name:** `DriverLab-E-26200`  
**Mode:** `VBoxManage startvm --type headless` (no guest window on your desktop)

## Why VirtualBox

Win11 **Home** — no Hyper-V. VirtualBox runs headless.

**Prerequisite:** **Intel VT-x / AMD-V enabled in firmware** (VBox error `VERR_VMX_MSR_ALL_VMX_DISABLED` if off). Reboot → BIOS → enable virtualization → retry `install-vm-unattended.ps1`.

## Exact build match

Host is **26200.9974** (25H2). Matching kernel requires the **same ISO channel** (Insider if you use Insider on host). For primitive hunt, **24H2 retail (26100)** in the VM is usually enough for CLFS/win32k **shape**; diff patches before claiming parity.

## Setup (one time)

```powershell
cd driver-lab\vm
# ISO: media\Win11.iso (download-win11-iso.ps1)
.\install-vm-unattended.ps1
# or manual: .\setup-vbox-headless-lab.ps1 -IsoPath ... -Start
```

Default local account: `DriverLab` / password from `install-vm-unattended.ps1` (change before snapshot).

Complete Windows install in headless VM (connect once via RDP if needed):

```powershell
# Optional: enable VRDP for first install only
& "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe" modifyvm DriverLab-E-26200 --vrde on --vrde-port 3389
```

Install **Guest Additions** → shared folder `D:\hostdev` → Development tree.

### HijackDriver (prove map + R/W in one click)

**Docs:** `hijack-driver\README.md` (build, IOCTL read/write, profiles).

```powershell
# Host: build
.\driver-lab\vm\hijack-driver\build.ps1

# Guest (elevated): after GA + D:\hostdev mounted
D:\hostdev\driver-lab\vm\bin\HijackDriver.exe --profile rust --map

# Host triggers guest (GA + logged-in DriverLab admin):
.\driver-lab\vm\deploy-run-hijack-driver-guest.ps1 -Profile rust
```

Inside guest (Admin):

```powershell
D:\hostdev\driver-lab\vm\guest-hunt-bootstrap.ps1
```

Snapshot:

```powershell
.\host-vm-headless.ps1 -Action snapshot -SnapshotName clean
```

## Live EAC/BE parity in guest

VmLab is optional hello-IOCTL only. **Use Betest:**

```powershell
D:\hostdev\driver-lab\vm\setup-vm-lab-betest.ps1 -Mode bypass -Profile rust
```

See `docs/26_vm_kernel_driver_lab.md`.

## Host automation (no manual login)

```powershell
.\wait-and-bootstrap-guest.ps1   # polls GA, runs setup-guest-remote + hijack probe
.\host-guest-exec.ps1 -Command @('powershell','-NoProfile','-Command','hostname')
```

Optional MCP: `mcp/README.md` + `lab.credentials.json` (gitignored).

**Elevated job queue (stomp / mapper):** `bg/README.md` + `host-bg-admin.ps1` on host — guest share `Z:\driver-lab\vm\bg`.

## Daily hunt (host)

```powershell
.\host-vm-headless.ps1 -Action start
# ... fuzz in guest ...
.\host-vm-headless.ps1 -Action stop
```

Zero-day hunting = guest crash dumps + `exploit/notes/`, not automatic in this repo.
