# After new driver2 AC dumps land — refresh all lane_d.by_profile build ids.
param([switch]$WhatIf)

$Driver3 = $PSScriptRoot
if ($WhatIf) {
    . (Join-Path $Driver3 'lib\ProfileDump.ps1')
    foreach ($p in @('rust','r6','arksa','deltaforce','arenabreakout')) {
        $d = Get-ProfileDumpDir $p
        $b = Get-ProfileBuildIdFromDump $p
        Write-Host "$p -> $b ($d)"
    }
    exit 0
}
& (Join-Path $Driver3 'sync-build-id-from-dumps.ps1') -Game rust -AllTitles
exit $LASTEXITCODE
