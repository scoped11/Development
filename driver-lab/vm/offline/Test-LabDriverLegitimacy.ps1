# O0 rubric: does the staged driver present as legitimately signed on this machine (lab PKI)?
param(
    [Parameter(Mandatory)][string]$SysPath,
    [string]$VmRoot = (Join-Path $PSScriptRoot '..'),
    [switch]$RequireCiSupplement,
    [string]$OutJson = ''
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path $SysPath)) { throw "Missing $SysPath" }

$stagedMeta = Join-Path $VmRoot 'vault\staged\driverlab-ci.json'
$checks = [ordered]@{}

$sig = Get-AuthenticodeSignature -FilePath $SysPath
$checks.authenticode_status = $sig.Status.ToString()
$checks.authenticode_ok = ($sig.Status -eq 'Valid')
if ($sig.SignerCertificate) {
    $checks.signer = $sig.SignerCertificate.Subject
    $checks.issuer = $sig.SignerCertificate.Issuer
    $checks.thumbprint = $sig.SignerCertificate.Thumbprint
} else {
    $checks.signer = $null
    $checks.issuer = $null
    $checks.thumbprint = $null
}

$tsLine = bcdedit /enum '{current}' 2>$null | Select-String 'testsigning'
$niLine = bcdedit /enum '{current}' 2>$null | Select-String 'nointegritychecks'
$checks.testsigning_no = -not ($tsLine -match 'Yes')
$checks.nointegritychecks_no = -not ($niLine -match 'Yes')

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
if ($signtool) {
    & $signtool.FullName verify /pa /ph $SysPath 2>&1 | Out-Null
    $checks.signtool_verify_ph_ec = $LASTEXITCODE
    $checks.signtool_ok = ($LASTEXITCODE -eq 0)
} else {
    $checks.signtool_ok = $null
    $checks.signtool_verify_ph_ec = $null
}

$checks.ci_supplement_ok = $null
$checks.ci_policy_id = $null
if ($RequireCiSupplement -and (Test-Path $stagedMeta)) {
    $meta = Get-Content $stagedMeta -Raw | ConvertFrom-Json
    $checks.ci_policy_id = $meta.policy_id
    $ciTool = Join-Path $env:windir 'System32\CiTool.exe'
    if (Test-Path $ciTool) {
        $activeDir = Join-Path $env:windir 'System32\CodeIntegrity\CiPolicies\Active'
        $idBraced = '{' + ($meta.policy_id.Trim('{}').ToUpper()) + '}'
        $onDisk = Join-Path $activeDir "$idBraced.cip"
        $checks.ci_policy_on_disk = (Test-Path $onDisk)
        $listed = cmd /c "echo.|`"$ciTool`" -lp" 2>&1 | Out-String
        $id = ($meta.policy_id -replace '[{}]', '').ToUpper()
        $blocks = [regex]::Split($listed, '(?=Policy ID:)')
        $hit = $false
        foreach ($b in $blocks) {
            if ($b -notmatch [regex]::Escape($id)) { continue }
            $auth = $b -match 'Is Authorized:\s*true'
            $enf = $b -match 'Is Currently Enforced:\s*true'
            if ($auth -and $enf) { $hit = $true; break }
        }
        if (-not $hit) {
            $hit = ($checks.ci_policy_on_disk -and
                $listed -match 'DriverLabOfflineSupplemental' -and
                $listed -match 'Is Authorized:\s*true' -and
                $listed -match 'Is Currently Enforced:\s*true')
        }
        $checks.ci_supplement_ok = [bool]$hit
    } else {
        $checks.ci_supplement_ok = $false
    }
}

$checks.legitimacy_ok = $checks.authenticode_ok -and $checks.testsigning_no -and $checks.nointegritychecks_no -and
    (($checks.signtool_ok -eq $true) -or ($checks.signtool_ok -eq $null))
if ($RequireCiSupplement) {
    $checks.legitimacy_ok = $checks.legitimacy_ok -and ($checks.ci_supplement_ok -eq $true)
}

$report = [ordered]@{
    schema    = 'otl_legitimacy_v1'
    utc       = (Get-Date).ToUniversalTime().ToString('o')
    sys_path  = (Resolve-Path $SysPath).Path
    checks    = $checks
    ok        = [bool]$checks.legitimacy_ok
}

if (-not $OutJson) {
    $OutJson = Join-Path $VmRoot 'bg\otl-legitimacy.json'
}
$dir = Split-Path $OutJson -Parent
if ($dir -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
($report | ConvertTo-Json -Depth 6) | Set-Content $OutJson -Encoding utf8

if (-not $report.ok) { exit 1 }
exit 0