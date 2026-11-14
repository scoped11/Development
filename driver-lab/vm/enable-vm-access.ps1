# Host: VRDE + NAT port forwards for SSH / WinRM into the lab guest.
param([string]$VmName = 'DriverLab-E-26200')

$ErrorActionPreference = 'SilentlyContinue'
$VBox = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
$state = & $VBox showvminfo $VmName --machinereadable | Select-String 'VMState=' | ForEach-Object { $_ -replace 'VMState="(.*)"', '$1' }

if ($state -eq 'running') {
    & $VBox controlvm $VmName vrde on 2>$null | Out-Null
    foreach ($rule in @(
            'guest-ssh,tcp,127.0.0.1,2222,,22'
            'guest-winrm,tcp,127.0.0.1,55985,,5985'
        )) {
        $id = $rule.Split(',')[0]
        & $VBox controlvm $VmName natpf1 delete $id 2>$null | Out-Null
        & $VBox controlvm $VmName natpf1 $rule
    }
} else {
    & $VBox modifyvm $VmName --vrde on --vrde-port 3389 --vrde-multicon on --vrde-auth-type null
    foreach ($rule in @(
            'guest-ssh,tcp,127.0.0.1,2222,,22'
            'guest-winrm,tcp,127.0.0.1,55985,,5985'
        )) {
        $id = $rule.Split(',')[0]
        & $VBox modifyvm $VmName --natpf1 delete $id 2>$null | Out-Null
        & $VBox modifyvm $VmName --natpf1 $rule
    }
}

Write-Host "[+] RDP: mstsc /v:127.0.0.1:3389 (enable VRDE in VM settings if needed)"
Write-Host "[+] SSH: ssh -p 2222 DriverLab@127.0.0.1 (after setup-guest-remote.ps1)"
Write-Host "[+] WinRM: winrs -r:http://127.0.0.1:55985 -u:DriverLab -p:*** cmd"
