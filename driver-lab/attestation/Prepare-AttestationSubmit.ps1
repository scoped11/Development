# Host: build release driver package for Hardware Dev Center attestation (pre-sign upload).
$ErrorActionPreference = 'Stop'
$AttestRoot = $PSScriptRoot
$Repo = Resolve-Path (Join-Path $AttestRoot '..\..')
$Byovd = Join-Path $Repo 'byovd'
$Out = Join-Path $AttestRoot 'submit-staging'
New-Item -ItemType Directory -Force -Path $Out | Out-Null

$build = Join-Path $Byovd 'driver2\build.ps1'
& $build
if ($LASTEXITCODE -ne 0) { throw 'driver2 build failed' }

$sys = Join-Path $Byovd 'driver2\bin\Driver2.sys'
if (-not (Test-Path $sys)) { throw 'Missing Driver2.sys (use full Driver2Build, not Stomp)' }
$inf = Join-Path $AttestRoot 'Driver2LabAttest.inf'
if (-not (Test-Path $inf)) { throw 'Missing Driver2LabAttest.inf' }

$drop = Join-Path $Out 'driver_package'
New-Item -ItemType Directory -Force -Path $drop | Out-Null
Copy-Item $sys (Join-Path $drop 'Driver2.sys') -Force
Copy-Item $inf (Join-Path $drop 'Driver2LabAttest.inf') -Force

$kitBin = Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\bin' -Recurse -Filter 'inf2cat.exe' -EA SilentlyContinue |
    Where-Object { $_.FullName -match '\\x86\\' } |
    Sort-Object { $_.Directory.Parent.Name } -Descending |
    Select-Object -First 1
if (-not $kitBin) { throw 'inf2cat.exe not found — install WDK' }

$catDir = Join-Path $Out 'catalog'
New-Item -ItemType Directory -Force -Path $catDir | Out-Null
& $kitBin.FullName /driver:$drop /os:10_X64 /verbose
$cat = Get-ChildItem $drop -Filter '*.cat' | Select-Object -First 1
if (-not $cat) { throw 'inf2cat did not produce .cat' }

$sha = (Get-FileHash $sys -Algorithm SHA256).Hash
@{
    schema       = 'attestation_submit_staging_v1'
    utc          = (Get-Date).ToUniversalTime().ToString('o')
  staging      = $Out
    driver_sha256 = $sha.ToUpper()
    files        = @(
        (Get-ChildItem $drop -File | ForEach-Object { $_.Name })
    )
    portal       = 'Upload driver_package (or portal zip) via Partner Center → Hardware → Driver signing'
    next         = @(
        'Sign .cat with EV cert on host (portal may re-sign)',
        'Submit attestation signing request',
        'Download signed drop → Import-AttestationPackage.ps1'
    )
} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $Out 'manifest.json') -Encoding UTF8

Write-Host "[+] Staged: $drop"
Write-Host "[+] Catalog: $($cat.FullName)"
Write-Host "[+] Manifest: $(Join-Path $Out 'manifest.json')"
Write-Host '[*] Complete EV + Partner Center enrollment, then upload via dashboard.'