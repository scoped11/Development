#Requires -Version 5.1
# What mapper vehicles exist on THIS PC + what to install (official sources only). Does not load drivers.
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Byovd = Split-Path $Root -Parent
$DevRoot = Split-Path $Byovd -Parent
$Policy = Join-Path $DevRoot 'driver-lab\data\ac_eac_be_vehicle_policy.json'

$build = [System.Environment]::OSVersion.Version.Build
$ubl = (Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion' -ErrorAction SilentlyContinue).UBR
$bl = (Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\CI\Config' -ErrorAction SilentlyContinue).VulnerableDriverBlocklistEnable
$hvci = (Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\DeviceGuard\Scenarios\HypervisorEnforcedCodeIntegrity' -ErrorAction SilentlyContinue).Enabled

Write-Host "=== Host $($env:COMPUTERNAME) | build $build.$ubl | blocklist=$bl hvci=$hvci ===" -ForegroundColor Cyan
Write-Host ""

& (Join-Path $Root 'scan-installed.ps1')
Write-Host ""

$catalog = @(
    @{ Name = 'CorsairLLAccess64.sys'; Driver = 'corsair'; Install = 'Corsair iCUE (official) — if you own Corsair RGB/fans' ; MapRisk = 'lower BSOD vs RTCore on Win11' }
    @{ Name = 'ThrottleStop.sys';      Driver = 'ts';      Install = 'ThrottleStop from https://www.techpowerup.com/download/techpowerup-throttlestop/' ; MapRisk = 'common lab choice' }
    @{ Name = 'SIVX64.sys';            Driver = 'siv';     Install = 'Gigabyte SIV / motherboard utility (Gigabyte boards)' ; MapRisk = 'only if Gigabyte toolkit installed' }
    @{ Name = 'RTCore64.sys';          Driver = 'rtcore';  Install = 'MSI Afterburner (you have this)' ; MapRisk = 'LOLDrivers; EAC/BE block; BSOD risk on 25H2' }
    @{ Name = 'TRIXX.sys';             Driver = 'trixx';   Install = 'Sapphire TRIXX utility' ; MapRisk = 'high BSOD risk' }
)

Write-Host 'Mapper-supported vehicles (install then run fetch-vehicle.ps1):' -ForegroundColor Cyan
foreach ($c in $catalog) {
    $veh = Join-Path $Root $c.Name
    $bin = Join-Path (Split-Path $Root -Parent) "bin\$($c.Name)"
    $onDisk = (Test-Path $veh) -or (Test-Path $bin) -or (
        Get-ChildItem -Path @($env:ProgramFiles, ${env:ProgramFiles(x86)}) -Filter $c.Name -Recurse -Depth 10 -EA SilentlyContinue | Select-Object -First 1
    )
    $st = if ($onDisk) { 'READY' } else { 'MISSING' }
    Write-Host ("  [{0}] {1,-26} --driver {2}" -f $st, $c.Name, $c.Driver)
    Write-Host "        $($c.Install)"
    Write-Host "        $($c.MapRisk)"
}

Write-Host ''
Write-Host 'Recommended for Win11 26200+ on this box (no other .sys found):' -ForegroundColor Green
Write-Host '  1) Install ThrottleStop (official) OR Corsair iCUE if you have Corsair hardware'
Write-Host '  2) powershell -File vehicles\fetch-vehicle.ps1'
Write-Host '  3) Map with: Driver2Runner --driver ts   (or corsair) — auto prefers these over RTCore'
Write-Host ''
Write-Host 'Rob Run (EAC): expect RTCore/Nal blocked at runtime even if map succeeds locally.'
Write-Host 'Lab-only map: close game; quit Afterburner after map (vehicle auto-stops).'
