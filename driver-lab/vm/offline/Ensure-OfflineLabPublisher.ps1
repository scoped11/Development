# Offline lab code-signing cert (no network). Host or elevated guest.
# Output: driver-lab\vm\vault\certs\DriverLabOffline.pfx + thumbprint in lab-publisher.json
$ErrorActionPreference = 'Stop'
$Vault = Join-Path $PSScriptRoot '..\vault'
$CertDir = Join-Path $Vault 'certs'
$Meta = Join-Path $CertDir 'lab-publisher.json'
New-Item -ItemType Directory -Force -Path $CertDir | Out-Null

$cn = 'CN=DriverLab Offline Lab Publisher'
$existing = Get-ChildItem Cert:\LocalMachine\My -ErrorAction SilentlyContinue |
    Where-Object { $_.Subject -eq $cn -and $_.NotAfter -gt (Get-Date) } |
    Select-Object -First 1

if (-not $existing) {
    $existing = New-SelfSignedCertificate -Type CodeSigningCert -Subject $cn `
        -KeyAlgorithm RSA -KeyLength 2048 -HashAlgorithm SHA256 `
        -CertStoreLocation Cert:\LocalMachine\My `
        -KeyExportPolicy Exportable -NotAfter (Get-Date).AddYears(5)
}

$rootStore = New-Object System.Security.Cryptography.X509Certificates.X509Store('Root', 'LocalMachine')
$rootStore.Open('ReadWrite')
if (-not ($rootStore.Certificates | Where-Object { $_.Thumbprint -eq $existing.Thumbprint })) {
    $rootStore.Add($existing)
}
$rootStore.Close()

$pubStore = New-Object System.Security.Cryptography.X509Certificates.X509Store('TrustedPublisher', 'LocalMachine')
$pubStore.Open('ReadWrite')
if (-not ($pubStore.Certificates | Where-Object { $_.Thumbprint -eq $existing.Thumbprint })) {
    $pubStore.Add($existing)
}
$pubStore.Close()

$pfxPath = Join-Path $CertDir 'DriverLabOffline.pfx'
$pw = ConvertTo-SecureString -String 'DriverLab-Offline-Vault-Only' -Force -AsPlainText
Export-PfxCertificate -Cert $existing -FilePath $pfxPath -Password $pw -Force | Out-Null

@{
    thumbprint = $existing.Thumbprint
    subject    = $existing.Subject
    pfx        = $pfxPath
    utc        = (Get-Date).ToUniversalTime().ToString('o')
    method     = 'offline_lab_publisher_v1'
} | ConvertTo-Json | Set-Content $Meta -Encoding UTF8

Write-Host "[+] Offline publisher thumbprint=$($existing.Thumbprint)"
Write-Host "[+] Meta: $Meta"