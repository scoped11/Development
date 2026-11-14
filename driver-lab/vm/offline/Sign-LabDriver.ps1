# Sign VmLabDrv.sys with offline vault cert (signtool, no timestamp server).
param(
    [Parameter(Mandatory)][string]$SysPath,
    [string]$VaultRoot = (Join-Path $PSScriptRoot '..\vault'),
    [switch]$SkipIfNoTool
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Ensure-OfflineLabPublisher.ps1')

$meta = Get-Content (Join-Path $VaultRoot 'certs\lab-publisher.json') -Raw | ConvertFrom-Json
$pfx = $meta.pfx
$pw = 'DriverLab-Offline-Vault-Only'

$kits = @(
    (Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'),
    (Join-Path $env:ProgramFiles 'Windows Kits\10\bin')
)
$signtool = $null
foreach ($bin in $kits) {
    if (-not (Test-Path $bin)) { continue }
    $signtool = Get-ChildItem -Path $bin -Recurse -Filter 'signtool.exe' -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\x64\\' } |
        Sort-Object { $_.Directory.Parent.Name } -Descending |
        Select-Object -First 1
    if ($signtool) { break }
}
$stagedTool = Join-Path $VaultRoot 'tools\signtool.exe'
if (-not $signtool -and (Test-Path $stagedTool)) { $signtool = Get-Item $stagedTool }
if (-not $signtool) {
    if ($SkipIfNoTool) { Write-Warning 'signtool missing — leaving image unsigned'; return }
    throw 'signtool.exe not found (install Windows SDK)'
}

# /ph page hashes — full Authenticode for kernel CI (trust via lab CA, not testsigning BCD).
& $signtool.FullName sign /v /fd SHA256 /f $pfx /p $pw /ph /d "VmLab kernel driver (lab)" $SysPath
if ($LASTEXITCODE -ne 0) { throw "signtool failed $LASTEXITCODE" }
Write-Host "[+] Signed $SysPath"