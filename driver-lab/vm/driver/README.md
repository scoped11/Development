# VmLabDrv — optional service-loaded lab driver

Small **WDK** driver for IOCTL practice inside the VM — **no** BYOVD mapper, **no** anti-cheat IOCTL surface.

| File | Role |
|------|------|
| `VmLabDrv.c` | Kernel driver → device `\\.\VmLab0` |
| `VmLabClient.cpp` | Usermode: session auth + marker read |
| `..\build-vm-lab-driver.ps1` | Builds `..\bin\VmLabDrv.sys` + `VmLabClient.exe` |
| `..\install-vm-lab-driver.ps1` | Creates kernel service (guest Admin) |

**IOCTLs** (match Betest prove codes for learning):

- `0x870` — session auth  
- `0x805` — read process memory  
- `0x806` — write process memory  

For production lab parity with captured titles, use **`byovd`** + **`hijack-driver\`** instead (`..\hijack-driver\README.md`).
