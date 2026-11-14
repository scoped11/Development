# Run once as Admin after you enable VT-x in UEFI. Registers RunOnce -> unattended install.
#Requires -RunAsAdministrator
$install = 'C:\Users\baile\OneDrive\Desktop\Development\driver-lab\vm\install-vm-unattended.ps1'
$cmd = "powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$install`""
New-ItemProperty -Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\RunOnce' `
    -Name 'DriverLabVtXInstall' -PropertyType String -Value $cmd -Force | Out-Null
Write-Host "[+] RunOnce registered. Next Windows logon starts DriverLab-E-26200 install."
