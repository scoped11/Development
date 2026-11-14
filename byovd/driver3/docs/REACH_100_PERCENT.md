# Reach 100% Scale A + Scale S (this PC + VM)

## This PC (host)

```powershell
cd C:\Users\baile\OneDrive\Desktop\Development\byovd\driver3
.\setup-host-driver3.ps1
.\complete-100-percent.ps1 -AllowStaticApproval
```

Read/write on **this machine**: [HOST_PC_READ_WRITE.md](HOST_PC_READ_WRITE.md)

## VM (one-time elevated)

1. RDP: `mstsc /v:127.0.0.1:3389` — user `DriverLab`
2. Elevated PowerShell:

```powershell
Z:\byovd\driver3\vm-enable-lab-uac-off.ps1
# reboot guest
Z:\byovd\driver3\register-driver3-elevated-task.ps1
Z:\byovd\driver3\setup-vm-driver3.ps1
Z:\byovd\driver3\host-b.ps1 -Profile rust -Execute -ForceMap -SkipProve
```

Read/write in **guest**: [VM_GUEST_READ_WRITE.md](VM_GUEST_READ_WRITE.md)

## Host finishes S tier

```powershell
.\vm-guest-hostb-from-host.ps1 -Profile rust -ForceMap -SkipProve
.\sync-guest-artifacts.ps1 -AllKernelProofs -IncludeInspector
.\complete-100-percent.ps1
```

Success: `dashboard.json` → `score_pct: 100`, `s_tier.json` → `tier: "S"`, `s_tier: true`.
