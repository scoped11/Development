# Run dma offset validator against latest Development/dumps/<date>/dump.cs
param(
    [string]$OutJson = '',
    [string]$DumpRoot = ''
)

$ErrorActionPreference = 'Stop'
$Driver3 = $PSScriptRoot
$DevRoot = Split-Path (Split-Path $Driver3 -Parent) -Parent
if (-not $OutJson) { $OutJson = Join-Path $Driver3 'offsets_validation.json' }

$dumps = if ($DumpRoot) { $DumpRoot } else { Join-Path $DevRoot 'dumps' }
$latest = Get-ChildItem -Path $dumps -Directory -ErrorAction SilentlyContinue |
    Where-Object { Test-Path (Join-Path $_.FullName 'dump.cs') } |
    Sort-Object Name -Descending | Select-Object -First 1

$py = Join-Path $DevRoot 'dma\scripts\validate_offsets_against_dump.py'
$result = [ordered]@{
    schema   = 'offsets_validation_v1'
    utc      = (Get-Date).ToUniversalTime().ToString('o')
    ok       = $false
    dump_dir = $null
    exit_code = 1
    stdout   = ''
    stderr   = ''
}

if (-not $latest) {
    $result.detail = "no dumps/*/dump.cs under $dumps"
    ($result | ConvertTo-Json -Depth 5) | Set-Content $OutJson -Encoding utf8
    Write-Host "[!] $($result.detail)"
    exit 4
}

$result.dump_dir = $latest.FullName
$env:DUMP_CS_OVERRIDE = Join-Path $latest.FullName 'dump.cs'
$exe = $null
foreach ($c in @('python', 'py')) {
    if (Get-Command $c -ErrorAction SilentlyContinue) { $exe = $c; break }
}
if (-not $exe -or -not (Test-Path $py)) {
    $result.detail = 'python or validate_offsets_against_dump.py missing'
    ($result | ConvertTo-Json -Depth 5) | Set-Content $OutJson -Encoding utf8
    exit 3
}

# Patcher: script uses hardcoded DUMP path — symlink/copy not needed if we pass via env; read if script supports argv
$dumpCs = Join-Path $latest.FullName 'dump.cs'
$p = Start-Process -FilePath $exe -ArgumentList @($py, $dumpCs) -Wait -PassThru -NoNewWindow -RedirectStandardOutput (Join-Path $env:TEMP 'ov_out.txt') -RedirectStandardError (Join-Path $env:TEMP 'ov_err.txt')
$result.exit_code = $p.ExitCode
$result.stdout = Get-Content (Join-Path $env:TEMP 'ov_out.txt') -Raw -ErrorAction SilentlyContinue
$result.stderr = Get-Content (Join-Path $env:TEMP 'ov_err.txt') -Raw -ErrorAction SilentlyContinue
$result.ok = ($p.ExitCode -eq 0)
if ($result.stdout -match 'mismatch|fail') { $result.ok = $false }

($result | ConvertTo-Json -Depth 5) | Set-Content $OutJson -Encoding utf8
Write-Host "[+] offsets validation ok=$($result.ok) dump=$($latest.Name)"
exit $(if ($result.ok) { 0 } else { 1 })
