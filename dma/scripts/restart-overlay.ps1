# Relaunch RobRunDMA only — never touches ROB RUN.exe (DMA attaches to the live process).
param(
    [ValidateSet("Debug", "Build")]
    [string]$Section = "Debug",
    [switch]$ShowConsole
)

$ErrorActionPreference = "Stop"
$dma = Split-Path $PSScriptRoot -Parent
$dev = Split-Path $dma -Parent
$sectionDir = if ($Section -eq "Build") { "build" } else { "debug" }
$exe = Join-Path $dev "$sectionDir\RobRunDMA.exe"

if ($Section -eq "Build" -and -not (Test-Path $exe)) {
    Write-Error "build/ is empty until approved. Use -Section Debug or: .\dma\scripts\build-windows.ps1 -Section Build -IApprove"
}
if (-not (Test-Path $exe)) {
    Write-Error "Build RobRunDMA first: .\dma\scripts\build-windows.ps1"
}

$game = Get-Process -Name "ROB RUN" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $game) {
    Write-Warning "ROB RUN is not running — start the game first, then run this script."
}

Stop-Process -Name "RobRunDMA" -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 500

$wd = Split-Path $exe -Parent
$cfgSrc = Join-Path $dma "config.json"
if (Test-Path $cfgSrc) {
    Copy-Item $cfgSrc (Join-Path $wd "config.json") -Force
}
$cursorDir = Join-Path $dev ".cursor"
if (-not (Test-Path $cursorDir)) { New-Item -ItemType Directory -Path $cursorDir | Out-Null }
$logOut = Join-Path $cursorDir "robrun-stdout.log"
$logErr = Join-Path $cursorDir "robrun-stderr.log"
"" | Set-Content $logOut
"" | Set-Content $logErr

$procArgs = @{
    FilePath               = $exe
    WorkingDirectory       = $wd
    RedirectStandardOutput = $logOut
    RedirectStandardError  = $logErr
    PassThru               = $true
}
if ($ShowConsole) {
    $env:ROBRUN_DEBUG_CONSOLE = "1"
    Remove-Item Env:ROBRUN_LOG_STDOUT -ErrorAction SilentlyContinue
    Remove-Item Env:ROBRUN_LOG_STDERR -ErrorAction SilentlyContinue
} else {
    Remove-Item Env:ROBRUN_DEBUG_CONSOLE -ErrorAction SilentlyContinue
    $env:ROBRUN_LOG_STDOUT = $logOut
    $env:ROBRUN_LOG_STDERR = $logErr
}
$proc = Start-Process @procArgs

Write-Host "Started: $exe (pid $($proc.Id))"
Write-Host "Console log: $logOut"
if ($game) { Write-Host "Attached target pid $($game.Id) (game was already running)." }
