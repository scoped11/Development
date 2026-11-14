# Kernel Lab entry — no FPGA. Orchestrates VM Host B; your cheat uses Driver2 after map.
param(
    [string]$Game = 'rust',
    [switch]$Execute,
    [switch]$FromHost
)

$Driver3 = $PSScriptRoot
. (Join-Path $Driver3 'lib\Get-Driver3LiveReadMethod.ps1')
$m = Get-Driver3LiveReadMethod
if ($m.method -ne 'kernel_lab_s') {
    Write-Host '[!] live_read_method is dma — run: set-live-read-method.ps1 -Method kernel_lab_s'
    if (-not $Execute) { exit 0 }
    throw 'Wrong live read method'
}

. (Join-Path $Driver3 'Resolve-Driver3Game.ps1')
$profile = Resolve-Driver3Profile $Game

Write-Host '=== driver3 Kernel Lab (no FPGA) ==='
Write-Host "Profile: $profile"
Write-Host 'Map + play in VM — Driver2Runner, then your EXE or Driver2Client'

& (Join-Path $Driver3 'lane-gate.ps1') -Game $profile | Out-Null
& (Join-Path $Driver3 'patch-build-ids.ps1') | Out-Null

. (Join-Path $Driver3 'lib\Test-Driver3KernelLabReady.ps1')
$ready = Test-Driver3KernelLabReady -Profile $profile
$ready.checks | ForEach-Object { Write-Host ("  [{0}] {1}" -f $(if ($_.pass) { '+' } else { '!' }), $_.name) }

if (-not $Execute) {
    Write-Host '[*] PLAN — -Execute to queue VM host-b from host; map inside VM for play'
    Write-Host '    drivers/route-kernel-lab.md'
    exit $(if ($ready.ok) { 0 } else { 5 })
}

if (-not $ready.ok) {
    $ready.issues | ForEach-Object { Write-Host "[!] $_" }
    throw 'Kernel lab not ready'
}

if ($FromHost) {
    & (Join-Path $Driver3 'vm-guest-hostb-from-host.ps1') -Profile $profile -ForceMap -SkipProve
}
Write-Host '[+] After host-b exit=0 in VM: run your cheat with Driver2Session'
exit $LASTEXITCODE
