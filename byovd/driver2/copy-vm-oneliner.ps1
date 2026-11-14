Set-Clipboard @'
$env:DRIVER2_VM_LAB_GUEST='1'
$env:DRIVER2_VM_REGISTER_TASK='1'
Set-Location Z:\byovd\driver2
powershell -ExecutionPolicy Bypass -File .\vm-lab.ps1
'@
Start-Process mstsc.exe -ArgumentList '/v:127.0.0.1:3389'
Write-Host '[+] RDP opening. Login: DriverLab / LabVM-ChangeMe!'
Write-Host '[+] One-liner copied — VM: PowerShell Run as administrator → paste → Enter'
