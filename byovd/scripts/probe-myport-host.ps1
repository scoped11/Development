# Elevated host smoke: load MyPortIO_x64.sys and phys write RMW via VehiclePhysProbe.exe
#Requires -RunAsAdministrator
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$probe = Join-Path $root 'byovd\bin\VehiclePhysProbe.exe'
$sys = Join-Path $root 'byovd\vehicles\MyPortIO_x64.sys'
if (-not (Test-Path $probe)) { throw "Missing $probe" }
if (-not (Test-Path $sys)) { throw "Missing $sys" }
& $probe myport $sys
exit $LASTEXITCODE
