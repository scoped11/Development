param([switch]$VmMap, [switch]$SkipVm)
& (Join-Path $PSScriptRoot 'reach-scale-s.ps1') -AllTitles -VmMap:$VmMap -SkipVm:$SkipVm
exit $LASTEXITCODE
