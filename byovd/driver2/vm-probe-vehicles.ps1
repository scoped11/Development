# VM only — elevated. RwTest map per --driver (no fallback). Writes driver2\vm_vehicle_probe.json
param(
    [string[]]$Drivers = @('rtcore', 'corsair', 'ts'),
    [string]$ByovdRoot = 'Z:\byovd',
    [int]$TimeoutSec = 120,
    [int]$StallSec = 45
)

$ErrorActionPreference = 'Stop'
$Driver2 = Join-Path $ByovdRoot 'driver2'
$logLocal = 'C:\DriverLab\vm_vehicle_probe.log'
$log = Join-Path $Driver2 'vm_vehicle_probe.log'
New-Item -ItemType Directory -Force -Path 'C:\DriverLab' | Out-Null
function L($m) {
    $t = "$(Get-Date -Format o) $m"
    Add-Content $logLocal $t -ErrorAction SilentlyContinue
    Add-Content $log $t -ErrorAction SilentlyContinue
    Write-Host $t
}

$adm = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $adm) {
    L 'BLOCKED: run via Driver3ElevatedWorker (elevated)'
    exit 3
}

$stage = 'C:\DriverLab\map'
New-Item -ItemType Directory -Force -Path $stage | Out-Null
if ($Drivers -contains 'corsair') {
    cmd /c 'sc stop CorsairLLAccess' | Out-Null
    cmd /c 'sc delete CorsairLLAccess' | Out-Null
    Start-Sleep -Seconds 3
}
foreach ($f in @('mapper_nal.exe', 'RwTest.sys', 'ThrottleStop.sys', 'RTCore64.sys', 'CorsairLLAccess64.sys', 'MsIo64.sys', 'MyPortIO_x64.sys', 'SIVX64.sys', 'NalDrvIntel.sys')) {
    $src = Join-Path $ByovdRoot "bin\$f"
    if (-not (Test-Path $src)) { $src = Join-Path $ByovdRoot "vehicles\$f" }
    if (Test-Path $src) { Copy-Item $src (Join-Path $stage $f) -Force }
}
$mapper = Join-Path $stage 'mapper_nal.exe'
$payload = Join-Path $stage 'RwTest.sys'
if (-not (Test-Path $mapper)) { throw "Missing $mapper" }
if (-not (Test-Path $payload)) { throw "Missing $payload" }

$results = [System.Collections.Generic.List[object]]::new()
Set-Location $stage

function Invoke-MapperProbe([string]$driver) {
    $stdout = Join-Path $stage "probe-$driver-stdout.txt"
    $stderr = Join-Path $stage "probe-$driver-stderr.txt"
    Remove-Item $stdout, $stderr -Force -ErrorAction SilentlyContinue
    $args = @($payload, '--driver', $driver, '--no-auth', '--rw-only', '--quiet', '--no-fallback')
    $p = Start-Process -FilePath $mapper -ArgumentList $args -PassThru -WorkingDirectory $stage `
        -NoNewWindow -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    $stallAccum = 0
    $lastLen = -1
    while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 5
        $len = 0
        if (Test-Path $stdout) { $len = (Get-Item $stdout).Length }
        if ($len -eq $lastLen) { $stallAccum += 5 } else { $stallAccum = 0; $lastLen = $len }
        if ($stallAccum -ge $StallSec) {
            L "STALL driver=$driver ${StallSec}s bytes=$len — killing"
            Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
            return @{ exit = 124; ok = $false; tail = "STALL killed after ${StallSec}s" }
        }
    }
    if (-not $p.HasExited) {
        L "TIMEOUT driver=$driver ${TimeoutSec}s — killing"
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
        return @{ exit = 125; ok = $false; tail = "TIMEOUT ${TimeoutSec}s" }
    }
    $ec = if ($null -ne $p.ExitCode) { [int]$p.ExitCode } else { 1 }
    $out = @()
    if (Test-Path $stdout) { $out += Get-Content $stdout -ErrorAction SilentlyContinue }
    if (Test-Path $stderr) { $out += Get-Content $stderr -ErrorAction SilentlyContinue }
    $tail = ($out | Select-Object -Last 8) -join ' | '
    return @{ exit = $ec; ok = ($ec -eq 0); tail = $tail }
}

foreach ($d in $Drivers) {
    if ($d -match '[,\s]' -or $d -notmatch '^(rtcore|corsair|msio|myport|ts|trixx|siv|nal)$') {
        L "SKIP invalid driver token: $d"
        continue
    }
    L "=== probe --driver $d (timeout=${TimeoutSec}s stall=${StallSec}s) ==="
    $r = Invoke-MapperProbe $d
    L "exit=$($r.exit) tail=$($r.tail)"
    $results.Add([ordered]@{
            driver = $d
            exit   = $r.exit
            ok     = $r.ok
            tail   = $r.tail
        })
    Start-Sleep -Seconds 2
}

$winner = ($results | Where-Object { $_.ok } | Select-Object -First 1)
$report = [ordered]@{
    schema  = 'vm_vehicle_probe_v1'
    guest   = $env:COMPUTERNAME
    utc     = (Get-Date).ToUniversalTime().ToString('o')
    winner  = $(if ($winner) { $winner.driver } else { $null })
    results = @($results)
}
$outPath = Join-Path $Driver2 'vm_vehicle_probe.json'
($report | ConvertTo-Json -Depth 5) | Set-Content $outPath -Encoding utf8
L "[+] wrote $outPath winner=$($report.winner)"
exit $(if ($winner) { 0 } else { 1 })
