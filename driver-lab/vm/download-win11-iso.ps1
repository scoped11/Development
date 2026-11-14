# Downloads a Windows 11 x64 ISO from Microsoft CDN (no product key required for lab VM install).
param(
    [string]$OutFile = (Join-Path $PSScriptRoot 'media\Win11.iso'),
    [string]$Landing = 'https://go.microsoft.com/fwlink/?linkid=2208844'
)

$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path (Split-Path $OutFile) | Out-Null

if (Test-Path $OutFile) {
    $len = (Get-Item $OutFile).Length
    if ($len -gt 4GB) {
        Write-Host "[=] ISO already exists ($([math]::Round($len / 1GB, 2)) GB): $OutFile"
        exit 0
    }
    Remove-Item $OutFile -Force
}

function Get-IsoRedirectUrl {
    param([string]$Uri)
    $resp = Invoke-WebRequest -Uri $Uri -Method Head -MaximumRedirection 10 -UseBasicParsing
    $final = $resp.BaseResponse.ResponseUri.AbsoluteUri
    if ($final -notmatch '\.iso(\?|$)') {
        throw "Redirect did not land on an .iso URL: $final"
    }
    return $final
}

Write-Host "[*] Resolving ISO URL from Microsoft..."
$url = Get-IsoRedirectUrl -Uri $Landing
Write-Host "[+] $url"

Write-Host "[+] Downloading to $OutFile (curl, resumable)"
$curl = Get-Command curl.exe -ErrorAction Stop
& $curl.Source -fL --retry 5 --retry-delay 5 -C - -o $OutFile $url
if ($LASTEXITCODE -ne 0) {
    throw "curl exited with $LASTEXITCODE"
}

$hash = (Get-FileHash $OutFile -Algorithm SHA256).Hash
Write-Host "[+] Done. SHA256=$hash Size=$([math]::Round((Get-Item $OutFile).Length / 1GB, 2)) GB"
