<#
.SYNOPSIS
  Drivers worth IDA time: high BYOVD imports, NOT on LOLDrivers (public LOL = already blocked).
  Does not load drivers.

.EXAMPLE
  .\greenfield-driver-candidates.ps1
  .\greenfield-driver-candidates.ps1 -MinTier 2 -CopyToInbox
#>
param(
    [string]$AuditDir = (Join-Path $PSScriptRoot "..\audit"),
    [int]$MinTier = 2,
    [switch]$CopyToInbox,
    [switch]$IncludeWhcpSigned,
    [switch]$IncludeMicrosoftInbox
)

$StrongPrimitives = @(
    "MmMapIoSpace", "MmGetPhysicalAddress", "MmCopyMemory",
    "HalGetBusDataByOffset", "HalSetBusDataByOffset", "ZwOpenSection"
)

$heuristic = Get-ChildItem (Join-Path $AuditDir "driver_byovd_heuristic_*.csv") |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
$block = Get-ChildItem (Join-Path $AuditDir "driver_blocklist_report_*.csv") |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1

if (-not $heuristic) {
    Write-Error "Run enumerate-local-drivers.ps1 and check-driver-blocklists.ps1 first."
}

$hRows = Import-Csv $heuristic.FullName
$bByFile = @{}
if ($block) {
    Import-Csv $block.FullName | ForEach-Object { $bByFile[$_.FileName.ToLowerInvariant()] = $_ }
}

function Is-MicrosoftDefault([string]$signer) {
    if ($signer -match "Microsoft Corporation" -and $signer -notmatch "Hardware Compatibility Publisher") { return $true }
    return $false
}

$candidates = foreach ($row in $hRows) {
    $tier = [int]$row.Tier
    if ($tier -lt $MinTier) { continue }

    $hits = $row.DangerHits -split ';' | ForEach-Object { $_.Trim() } | Where-Object { $_ }
    $strong = @($hits | Where-Object { $_ -in $StrongPrimitives })
    if ($strong.Count -eq 0) { continue }

    $bl = $bByFile[$row.FileName.ToLowerInvariant()]
    if ($bl -and $bl.LOLDrivers -eq "yes") { continue }

    if (-not $IncludeMicrosoftInbox -and $row.ThirdParty -ne "True") { continue }

    $signer = $row.Signer
    if (-not $IncludeWhcpSigned -and (Is-MicrosoftDefault $signer)) { continue }

    $acBlock = $false
    if ($bl) {
        if ($bl.EAC_PaidTier_Heuristic -match "block|match" -or $bl.BE_PaidTier_Heuristic -match "block|match") {
            $acBlock = $true
        }
    }

    [pscustomobject]@{
        FileName   = $row.FileName
        Tier       = $tier
        DangerHits = $row.DangerHits
        State      = $row.State
        Signer     = $signer
        LOLDrivers = if ($bl) { $bl.LOLDrivers } else { "?" }
        Path       = $row.Path
        AcNameHit  = if ($acBlock) { "likely" } else { "unknown" }
        Why        = "non-LOL + import tier $tier"
    }
}

$stamp = Get-Date -Format "yyyy-MM-dd"
$out = Join-Path $AuditDir "greenfield_candidates_$stamp.csv"
$candidates | Sort-Object -Property @{ E = { -$_.Tier } }, FileName | Export-Csv $out -NoTypeInformation

$inbox = Join-Path $AuditDir "inbox"
if ($CopyToInbox) {
    New-Item -ItemType Directory -Force -Path $inbox | Out-Null
    foreach ($c in $candidates) {
        if (Test-Path -LiteralPath $c.Path) {
            Copy-Item -LiteralPath $c.Path -Destination (Join-Path $inbox $c.FileName) -Force -ErrorAction SilentlyContinue
        }
    }
}

Write-Host "LOLDrivers catalog = already blocked on Win11 policy + paid EAC/BE — skip for mapper."
Write-Host "Greenfield candidates (IDA inbox targets): $($candidates.Count)"
Write-Host "Wrote $out"
if ($candidates.Count -gt 0) {
    $candidates | Select-Object -First 20 FileName, Tier, State, AcNameHit, Signer | Format-Table -AutoSize
}
