# Mirror AC title captures from driver-lab/dumps into driver2/dumps (SDK bundle).
param(
    [ValidateSet('all', 'eac', 'be', 'ace')]
    [string]$Family = 'all',
    [string]$LabDumps = '',
    [string]$DestRoot = ''
)

$ErrorActionPreference = 'Stop'
$Driver2 = $PSScriptRoot
if (-not $DestRoot) { $DestRoot = Join-Path $Driver2 'dumps' }
if (-not $LabDumps) {
    $DevRoot = Split-Path (Split-Path $Driver2 -Parent) -Parent
    $LabDumps = Join-Path $DevRoot 'driver-lab\dumps'
}
if (-not (Test-Path $LabDumps)) { throw "Lab dumps not found: $LabDumps" }

$patterns = switch ($Family) {
    'eac' { @('rust-eac-eos_*') }
    'be'  { @('r6-battleye_*') }
    'ace' { @('deltaforce-ace_*', 'arenabreakout-ace_*') }
    default {
        @('rust-eac-eos_*', 'r6-battleye_*', 'deltaforce-ace_*', 'arenabreakout-ace_*')
    }
}

New-Item -ItemType Directory -Force -Path $DestRoot | Out-Null
$copied = @()

foreach ($pat in $patterns) {
    $hit = Get-ChildItem -Path $LabDumps -Directory -Filter $pat -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending | Select-Object -First 1
    if (-not $hit) {
        Write-Warning "No match for $pat under $LabDumps"
        continue
    }
    $dest = Join-Path $DestRoot $hit.Name
    robocopy $hit.FullName $dest /MIR /NFL /NDL /NJH /NJS /nc /ns /np | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy failed for $($hit.Name) exit $LASTEXITCODE" }
    $copied += $hit.Name
    Write-Host "[+] $($hit.Name) -> $dest"
}

if ($copied.Count -eq 0) { throw 'No dumps mirrored' }
Write-Host "[*] Mirrored $($copied.Count) bundle(s). Update profiles/*.json capture_dump if folder stamps changed."
