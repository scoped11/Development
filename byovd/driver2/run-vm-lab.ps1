# Double-click / one command: smoke VM, open RDP, copy elevated one-liner, then map when ready.
param(
    [ValidateSet('rust', 'r6')]
    [string]$Profile = 'rust',
    [switch]$SkipRdp,
    [switch]$ExecuteOnly
)

$Driver2 = $PSScriptRoot
$setup = Join-Path $Driver2 'VM_LAB_SETUP.md'

if (-not $ExecuteOnly) {
    Write-Host '=== Step 1: VM smoke test (from host) ==='
    & (Join-Path $Driver2 'vm-lab.ps1') -Profile $Profile
    if ($LASTEXITCODE -ne 0) { Write-Host "[!] smoke issue exit=$LASTEXITCODE"; exit $LASTEXITCODE }

    $oneLiner = @"
`$env:DRIVER2_VM_LAB_GUEST='1'; `$env:DRIVER2_VM_REGISTER_TASK='1'; Set-Location Z:\byovd\driver2; powershell -ExecutionPolicy Bypass -File .\vm-lab.ps1
"@

    if (-not $SkipRdp) {
        Write-Host ''
        Write-Host '=== Step 2: One-time VM setup (about 30 seconds) ==='
        Write-Host 'Opening Remote Desktop to the VM...'
        Start-Process 'mstsc.exe' -ArgumentList '/v:127.0.0.1:3389'
        Set-Clipboard -Value $oneLiner
        Write-Host '[+] Copied to clipboard — in the VM:'
        Write-Host '    1) Start → PowerShell → Run as administrator'
        Write-Host '    2) Right-click title bar → Paste → Enter'
        Write-Host '    3) Wait for: registered Driver2VmLabWorker'
        Write-Host ''
        Read-Host 'Press ENTER here when that is done (then host runs the map)'
    }
}

Write-Host ''
Write-Host "=== Step 3: Map Driver2 in VM (profile=$Profile) ==="
& (Join-Path $Driver2 'vm-lab.ps1') -Profile $Profile -Execute -ForceMap -SkipProve
$ec = $LASTEXITCODE
if ($ec -eq 24) {
    Write-Host ''
    Write-Host 'Still need Step 2 — see' $setup
}
elseif ($ec -eq 0) {
    Write-Host '[+] Done — see driver2\vm_lab_result.json'
}
exit $ec
