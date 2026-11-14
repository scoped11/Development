#Requires -RunAsAdministrator
# Run ONCE in DriverLab guest (elevated). Allows Guest Additions to map without interactive UAC.
Set-ItemProperty -Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System' -Name EnableLUA -Value 0 -Type DWord
Write-Host '[+] UAC disabled for lab VM — reboot guest, then register-driver3-elevated-task.ps1'
Write-Host '    Reboot: shutdown /r /t 5'
