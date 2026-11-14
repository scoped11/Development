# Host: import Microsoft-signed attestation drop into vault for DriverLab guest tests.
param(
    [Parameter(Mandatory)][string]$Folder,
    [string]$Vault = (Join-Path $PSScriptRoot '..\vm\vault\staged')
)

$ErrorActionPreference = 'Stop'
$sys = Get-ChildItem $Folder -Filter '*.sys' -Recurse | Select-Object -First 1
$cat = Get-ChildItem $Folder -Filter '*.cat' -Recurse | Select-Object -First 1
if (-not $sys) { throw "No .sys under $Folder" }

New-Item -ItemType Directory -Force -Path $Vault | Out-Null
Copy-Item $sys.FullName (Join-Path $Vault 'Driver2.attest.sys') -Force
if ($cat) { Copy-Item $cat.FullName (Join-Path $Vault 'Driver2.attest.cat') -Force }

$sig = Get-AuthenticodeSignature $sys.FullName
@{
    schema    = 'attestation_import_v1'
    utc       = (Get-Date).ToUniversalTime().ToString('o')
    sys       = $sys.FullName
    cat       = if ($cat) { $cat.FullName } else { $null }
    status    = $sig.Status.ToString()
    signer    = $sig.SignerCertificate.Subject
    thumbprint = $sig.SignerCertificate.Thumbprint
} | ConvertTo-Json | Set-Content (Join-Path $Vault 'attestation-import.json') -Encoding UTF8

Write-Host "[+] Staged $(Join-Path $Vault 'Driver2.attest.sys') — deploy on lab VM only."