Write-Host '[*] RDP to DriverLab guest — run elevated scripts in VM_GUEST_READ_WRITE.md'
Start-Process 'mstsc.exe' -ArgumentList '/v:127.0.0.1:3389'
