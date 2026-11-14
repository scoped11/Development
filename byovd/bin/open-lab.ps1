# Legacy shim — launchers live under driver2\
$d2 = Join-Path (Split-Path $PSScriptRoot -Parent) 'driver2'
& (Join-Path $d2 'open-lab.ps1')
