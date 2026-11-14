# Import host-staged publisher cert into guest trust (offline share only).
param([string]$VaultRoot = (Join-Path $PSScriptRoot '..\vault'))

$Meta = Join-Path $VaultRoot 'certs\lab-publisher.json'
$Pfx  = Join-Path $VaultRoot 'certs\DriverLabOffline.pfx'
if (-not (Test-Path $Pfx)) { return }
$pw = ConvertTo-SecureString 'DriverLab-Offline-Vault-Only' -AsPlainText -Force
Import-PfxCertificate -FilePath $Pfx -CertStoreLocation Cert:\LocalMachine\My -Password $pw -Exportable | Out-Null
Import-PfxCertificate -FilePath $Pfx -CertStoreLocation Cert:\LocalMachine\Root -Password $pw -Exportable | Out-Null
Import-PfxCertificate -FilePath $Pfx -CertStoreLocation Cert:\LocalMachine\TrustedPublisher -Password $pw | Out-Null
Import-PfxCertificate -FilePath $Pfx -CertStoreLocation Cert:\LocalMachine\TrustedPeople -Password $pw | Out-Null
$thumb = $null
if (Test-Path $Meta) { $thumb = (Get-Content $Meta -Raw | ConvertFrom-Json).thumbprint }
if (-not $thumb) {
    $thumb = (Get-PfxData -FilePath $Pfx -Password $pw).EndEntityCertificates[0].Thumbprint
}
$cer = Join-Path $env:TEMP "DriverLabPublisher-$thumb.cer"
if ($thumb -and -not (Test-Path $cer)) {
    $c = Get-ChildItem Cert:\LocalMachine\My\$thumb -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($c) {
        Export-Certificate -Cert $c -FilePath $cer -Force | Out-Null
        cmd /c "certutil -addstore -f Root `"$cer`"" 2>$null | Out-Null
        cmd /c "certutil -addstore -f TrustedPublisher `"$cer`"" 2>$null | Out-Null
    }
}
Write-Host "[+] Imported lab publisher (My/Root/TP/TrustedPeople)"