# Enable VT-x (required for DriverLab-E-26200)

`Virtualization Enabled In Firmware: No` on this host blocks **all** 64-bit VMs.

## In UEFI (12900K boards — names vary)

1. **Advanced** → **CPU Configuration** (or Overclocking → CPU Features)
2. **Intel Virtualization Technology** → **Enabled**
3. (Optional) **VT-d** → Enabled
4. Save & exit

Other labels: *Intel VT-x*, *Virtualization*, *SVM* (AMD).

## After reboot

Admin PowerShell:

```powershell
cd C:\Users\baile\OneDrive\Desktop\Development\driver-lab\vm
.\register-post-vtx-install.ps1
# or manually:
.\install-vm-unattended.ps1
```

When guest is ready: `D:\hostdev\driver-lab\vm\guest-hunt-bootstrap.ps1`
