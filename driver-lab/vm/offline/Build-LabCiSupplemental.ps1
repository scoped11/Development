# Host or guest: build supplemental .cip that allows the staged VmLabDrv publisher (no testsigning).
$ErrorActionPreference = 'Stop'
$Vault = Join-Path $PSScriptRoot '..\vault'
$Staged = Join-Path $Vault 'staged'
New-Item -ItemType Directory -Force -Path $Staged | Out-Null
$sys = Join-Path $Staged 'VmLabDrv.signed.sys'
if (-not (Test-Path $sys)) { throw "Missing $sys" }

if (-not (Get-Module -ListAvailable ConfigCI)) {
    throw 'ConfigCI module missing'
}
Import-Module ConfigCI -Force

$xmlWork = Join-Path $Staged 'driverlab-from-driver.xml'
$cip = Join-Path $Staged 'driverlab-supplemental.cip'

$rules = New-CIPolicyRule -Level Publisher -Fallback FilePublisher -DriverFilePath $sys
if (-not $rules) { throw 'New-CIPolicyRule returned nothing' }

New-CIPolicy -Rules $rules -FilePath $xmlWork -UserPEs:$false
$activeDir = Join-Path $env:windir 'System32\CodeIntegrity\CiPolicies\Active'
# 26200 lab VM enforces VerifiedAndReputableDesktopEvaluation, not the legacy 60FD… base.
$baseCandidates = @(
    '{1283AC0F-FFF1-49AE-ADA1-8A933130CAD6}.cip',
    '{60FD87F8-4593-44A0-91B0-2E0DA022F248}.cip'
)
$baseCip = $null
foreach ($rel in $baseCandidates) {
    $p = Join-Path $activeDir $rel
    if (Test-Path $p) { $baseCip = $p; break }
}
if (-not $baseCip) {
    $ciTool = Join-Path $env:windir 'System32\CiTool.exe'
    if (Test-Path $ciTool) {
        $listed = cmd /c "echo.|`"$ciTool`" -lp" 2>&1 | Out-String
        if ($listed -match 'Policy ID:\s*([0-9a-f\-]{36})[\s\S]*?Is Currently Enforced:\s*true[\s\S]*?Base Policy ID:\s*\1') {
            $id = $Matches[1].ToUpper()
            $guess = Join-Path $activeDir "{$id}.cip"
            if (Test-Path $guess) { $baseCip = $guess }
        }
    }
}
if (-not $baseCip) {
    $baseCip = (Get-ChildItem (Join-Path $activeDir '*.cip') -EA Stop |
        Where-Object { $_.Name -notmatch 'driverlab|6E00A72B|CD37C69B|CC9778E4' } |
        Select-Object -First 1).FullName
}
if (-not $baseCip) { throw 'No base CI policy .cip found under CiPolicies\Active' }
$baseId = ([System.IO.Path]::GetFileNameWithoutExtension($baseCip) -replace '[{}]', '').ToUpper()
Write-Host "[*] Supplement base policy: $baseId ($baseCip)"
$suppId = [guid]::NewGuid().ToString().ToUpper()
Set-CIPolicyIdInfo -FilePath $xmlWork -PolicyName 'DriverLabOfflineSupplemental' `
    -SupplementsBasePolicyID $baseId -BasePolicyToSupplementPath $baseCip -PolicyId $suppId
try { Set-RuleOption -FilePath $xmlWork -Option 3 -Delete } catch { }
ConvertFrom-CIPolicy -XmlFilePath $xmlWork -BinaryFilePath $cip

# .cip uses WDAC policy signing, not Authenticode signtool — deploy unsigned then authorize via CiTool update.

Write-Host "[+] Supplemental CI policy: $cip"
@{
    schema    = 'driverlab_ci_supplemental_v1'
    cip       = $cip
    policy_id = $suppId
    base_id   = $baseId
    utc       = (Get-Date).ToUniversalTime().ToString('o')
} | ConvertTo-Json | Set-Content (Join-Path $Staged 'driverlab-ci.json') -Encoding UTF8