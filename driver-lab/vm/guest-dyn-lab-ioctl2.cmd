@echo off
copy /Y Z:\driver-lab\vm\guest-dyn-lab-ioctl2.ps1 C:\re\guest-dyn-lab-ioctl2.ps1
C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\re\guest-dyn-lab-ioctl2.ps1
