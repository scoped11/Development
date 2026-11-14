#Requires -RunAsAdministrator
# Run inside guest after GA + shared folder. Enables SSH/WinRM for host automation + MCP bridge.
$ErrorActionPreference = 'Stop'

$marker = 'C:\DriverLab\remote-ready.json'
New-Item -ItemType Directory -Force -Path (Split-Path $marker) | Out-Null

# OpenSSH Server (optional host ssh -p 2222 after NAT forward)
$ssh = Get-WindowsCapability -Online | Where-Object Name -like 'OpenSSH.Server*'
if ($ssh.State -ne 'Installed') {
    Add-WindowsCapability -Online -Name $ssh.Name | Out-Null
}
Set-Service sshd -StartupType Automatic -ErrorAction SilentlyContinue
Start-Service sshd -ErrorAction SilentlyContinue
New-NetFirewallRule -DisplayName 'OpenSSH-Server-In-TCP' -Direction Inbound -Protocol TCP -LocalPort 22 -Action Allow -ErrorAction SilentlyContinue | Out-Null

# WinRM (basic auth, lab only)
Enable-PSRemoting -Force -SkipNetworkProfileCheck
Set-Item WSMan:\localhost\Client\TrustedHosts -Value '*' -Force -ErrorAction SilentlyContinue

$regElevated = @(
    'Z:\byovd\driver3\register-driver3-elevated-task.ps1',
    'D:\hostdev\byovd\driver3\register-driver3-elevated-task.ps1'
)
foreach ($r in $regElevated) {
    if (Test-Path $r) {
        Write-Host "[*] Registering Driver3ElevatedWorker from $r"
        & $r
        break
    }
}

$LabShare = @('Z:\driver-lab', 'D:\hostdev\driver-lab') | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($LabShare) {
    & (Join-Path $LabShare 'vm\guest-hunt-bootstrap.ps1')
    $betest = Join-Path $LabShare 'vm\setup-vm-lab-betest.ps1'
    if (Test-Path $betest) {
        # Baseline map only on first boot; full bypass after snapshot (26_vm_kernel_driver_lab.md).
        try {
            $env:VM_LAB_FULL_GATE = '1'
            & (Join-Path $LabShare 'vm\vm-lab-parity-gate.ps1') -Profile both -ForceMap
        } catch { Write-Warning "VM parity gate: $_" }
        $next = Join-Path $LabShare 'exploit\notes\vm-lab-next-steps.md'
        @'
# VM lab — live EAC/BE parity (after snapshot `clean`)

```powershell
D:\hostdev\driver-lab\vm\setup-vm-lab-betest.ps1 -Mode bypass -Profile rust
D:\hostdev\driver-lab\vm\setup-vm-lab-betest.ps1 -Mode bypass -Profile r6
D:\hostdev\driver-lab\vm\setup-vm-lab-betest.ps1 -Mode ioctl -Profile rust
```

Betest + `--ac-replica-bypass` = same preempt/neutralize ioctl surface as live research needs.
'@ | Set-Content $next -Encoding UTF8
    }
}

$build = (Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion')
@{
    remote_ready_utc = (Get-Date).ToUniversalTime().ToString('o')
    guest_build      = "$($build.CurrentBuildNumber).$($build.UBR)"
    sshd             = (Get-Service sshd -ErrorAction SilentlyContinue).Status
    winrm            = $true
} | ConvertTo-Json | Set-Content $marker -Encoding UTF8

Write-Host "[+] Guest remote ready. Marker: $marker"
