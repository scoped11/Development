# Borrowed-identity gate: on-disk module still vendor Authenticode (no graft).
param(
    [Parameter(Mandatory)][string]$SysPath,
    [string]$PublisherRegex = '',
    [string]$PriorSha256 = '',
    [string]$OutJson = '',
    [switch]$RequireLoaded
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $SysPath)) { throw "Missing $SysPath" }

$full = (Resolve-Path -LiteralPath $SysPath).Path
$sig = Get-AuthenticodeSignature -FilePath $full
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $full).Hash.ToUpper()
$checks = [ordered]@{
    authenticode_status = $sig.Status.ToString()
    authenticode_ok     = ($sig.Status -eq 'Valid')
    signer              = $null
    issuer              = $null
    thumbprint          = $null
    sha256              = $hash
    hash_unchanged      = $null
    publisher_ok        = $null
    loaded              = $null
}

if ($sig.SignerCertificate) {
    $checks.signer = $sig.SignerCertificate.Subject
    $checks.issuer = $sig.SignerCertificate.Issuer
    $checks.thumbprint = $sig.SignerCertificate.Thumbprint
}

if ($PublisherRegex) {
    $subj = [string]$checks.signer
    $checks.publisher_ok = [bool]($subj -match $PublisherRegex)
} else {
    $checks.publisher_ok = $true
}

if ($PriorSha256) {
    $checks.hash_unchanged = ($hash -eq $PriorSha256.ToUpper())
} else {
    $checks.hash_unchanged = $true
}

if ($RequireLoaded) {
    $leaf = [IO.Path]::GetFileName($full)
    $drv = Get-CimInstance Win32_SystemDriver -ErrorAction SilentlyContinue |
        Where-Object {
            $_.PathName -and (
                $_.PathName -like "*\$leaf" -or
                $_.PathName -like "*$leaf*"
            )
        } |
        Select-Object -First 1
    $checks.loaded = [bool]$drv
    if ($drv) {
        $checks.loaded_name = $drv.Name
        $checks.loaded_state = $drv.State
        $checks.loaded_path = $drv.PathName
    }
}

$checks.appearance_ok = $checks.authenticode_ok -and $checks.publisher_ok -and $checks.hash_unchanged -and
    ((-not $RequireLoaded) -or $checks.loaded)

$report = [ordered]@{
    schema   = 'signed_module_appearance_v1'
    utc      = (Get-Date).ToUniversalTime().ToString('o')
    sys_path = $full
    checks   = $checks
    ok       = [bool]$checks.appearance_ok
}

if ($OutJson) {
    $dir = Split-Path $OutJson -Parent
    if ($dir -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    ($report | ConvertTo-Json -Depth 6) | Set-Content $OutJson -Encoding utf8
}

$report | ConvertTo-Json -Depth 6 | Write-Host
if (-not $report.ok) { exit 1 }
exit 0
