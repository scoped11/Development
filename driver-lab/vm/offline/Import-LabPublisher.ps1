# Import host-staged publisher cert into guest trust (offline share only).
param([string]$VaultRoot = (Join-Path $PSScriptRoot '..\vault'))

$Meta = Join-Path $VaultRoot 'certs\lab-publisher.json'
$Pfx  = Join-Path $VaultRoot 'certs\DriverLabOffline.pfx'
if (-not (Test-Path $Pfx)) { return }
$pw = ConvertTo-SecureString 'DriverLab-Offline-Vault-Only' -AsPlainText -Force
Import-PfxCertificate -FilePath $Pfx -CertStoreLocation Cert:\LocalMachine\Root -Password $pw -Exportable | Out-Null
Import-PfxCertificate -FilePath $Pfx -CertStoreLocation Cert:\LocalMachine\TrustedPublisher -Password $pw | Out-Null
Write-Host "[+] Imported lab publisher from share"