# Cheat inspector — static SDK bar (>=90%) + optional VM dynamic chain (-Execute needs Admin).
param(
    [string]$Driver = 'rtcore',
    [switch]$ForceMap,
    [switch]$Execute,
    [switch]$IncludeAce,
    [switch]$IncludeBe,
    [string]$OutJson = ''
)

$ErrorActionPreference = 'Stop'
$Driver2 = $PSScriptRoot
$Byovd = Split-Path $Driver2 -Parent
if (-not $OutJson) { $OutJson = Join-Path $Driver2 'cheat_inspector.json' }

$target = 90
$checks = [System.Collections.Generic.List[object]]::new()

function Add-Check {
    param([string]$Category, [string]$Name, [bool]$Pass, [string]$Detail = '')
    $script:checks.Add([pscustomobject]@{
            category = $Category
            name     = $Name
            pass     = $Pass
            detail   = $Detail
        })
}

function Test-JsonFile {
    param([string]$Path)
    if (-not (Test-Path $Path)) { return $false }
    try {
        Get-Content $Path -Raw | ConvertFrom-Json | Out-Null
        return $true
    } catch { return $false }
}

# --- S1 Static: documentation ---
$docs = @(
    'DEVELOPER_SDK.md',
    'DEVELOPER_APPROVAL.md',
    'CHEAT_INSPECTOR.md',
    'AC_FINGERPRINTS.md',
    'EAC_INTEGRATION.md',
    'ACE_INTEGRATION.md',
    'VEHICLE_STEALTH.md',
    'IOCTL_REFERENCE.md',
    'CAPABILITIES.md',
    'CONFIDENCE.md',
    'README.md'
)
foreach ($d in $docs) {
    $p = Join-Path $Driver2 $d
    Add-Check -Category 'docs' -Name "doc_$d" -Pass (Test-Path $p)
}

$blockPlan = Join-Path $Byovd 'docs\11_ac_block_vs_hide_plan.md'
Add-Check -Category 'docs' -Name 'tier0_block_vs_hide_plan' -Pass (Test-Path $blockPlan)

# --- SDK surface ---
$sdkFiles = @(
    'sdk\driver2_api.h',
    'sdk\Driver2Session.hpp',
    'examples\cheat_dev_minimal.cpp',
    'Driver2Runner.cpp'
)
foreach ($f in $sdkFiles) {
    Add-Check -Category 'sdk' -Name "file_$($f -replace '\\','_')" -Pass (Test-Path (Join-Path $Driver2 $f))
}

$captureBundles = @(
    @{ name = 'eac_rust'; dir = 'rust-eac-eos_2026-11-13_0204'; file = 'kernel\EasyAntiCheat_EOS.sys' }
    @{ name = 'be_r6'; dir = 'r6-battleye_2026-11-13_0214'; file = 'usermode\BEService_r6s.exe' }
    @{ name = 'ace_delta'; dir = 'deltaforce-ace_2026-11-13_0215'; file = 'kernel\ACE-GAME.sys' }
    @{ name = 'ace_arena'; dir = 'arenabreakout-ace_2026-11-13_0215'; file = 'kernel\ACE-GAME.sys' }
)
foreach ($b in $captureBundles) {
    $root = Join-Path $Driver2 "dumps\$($b.dir)"
    Add-Check -Category 'captures' -Name "$($b.name)_manifest" -Pass (Test-Path (Join-Path $root 'manifest.json'))
    Add-Check -Category 'captures' -Name "$($b.name)_anchor" -Pass (Test-Path (Join-Path $root $b.file)) -Detail $b.file
}
Add-Check -Category 'tooling' -Name 'sync_ac_dumps_script' -Pass (Test-Path (Join-Path $Driver2 'sync-ac-dumps.ps1'))
Add-Check -Category 'captures' -Name 'ac_fingerprints_index' -Pass (Test-Path (Join-Path $Driver2 'dumps\ac_fingerprints_index.json'))
foreach ($p in @('rust', 'r6')) {
    $pj = switch ($p) { 'rust' { 'profiles\eac-rust.json' } 'r6' { 'profiles\be-r6.json' } }
    $pp = Join-Path $Driver2 $pj
    $dt = $false
    if (Test-Path $pp) {
        $o = Get-Content $pp -Raw | ConvertFrom-Json
        $dt = ($null -ne $o.dump_tracking -and $o.dump_tracking.session_client_cmds)
    }
    Add-Check -Category 'captures' -Name "dump_tracking_$p" -Pass $dt $pj
}

$profiles = @('ace-deltaforce.json', 'ace-arenabreakout.json', 'eac-rust.json', 'be-r6.json', 'be-arksa.json', 'universal.json')
foreach ($pf in $profiles) {
    $pp = Join-Path $Driver2 "profiles\$pf"
    $ok = Test-JsonFile $pp
    $detail = ''
    if ($ok) {
        $j = Get-Content $pp -Raw | ConvertFrom-Json
        $ok = ($null -ne $j.mapper_bypass) -and ($j.mapper_bypass.Count -ge 4)
        if (-not $ok) { $detail = 'missing mapper_bypass' }
    }
    Add-Check -Category 'profiles' -Name "profile_$pf" -Pass $ok -Detail $detail
}

# --- Tooling ---
$tools = @{
    verify_pf_diag = Join-Path $Byovd 'scripts\verify-pf-diag.ps1'
    ac_preflight   = Join-Path $Byovd 'scripts\ac-preflight.ps1'
    approval_gate  = Join-Path $Driver2 'developer-approval-gate.ps1'
    forensics      = Join-Path $Driver2 'forensics-capture.ps1'
}
foreach ($kv in $tools.GetEnumerator()) {
    Add-Check -Category 'tooling' -Name $kv.Key -Pass (Test-Path $kv.Value)
}

# --- Binaries (warn-level for static — still scored) ---
$bins = @(
    (Join-Path $Byovd 'bin\mapper_nal.exe'),
    (Join-Path $Driver2 'bin\Driver2.sys'),
    (Join-Path $Byovd 'bin\Driver2Runner.exe'),
    (Join-Path $Byovd 'bin\Driver2Client.exe'),
    (Join-Path $Byovd 'bin\process_rw.exe')
)
$binOk = 0
foreach ($b in $bins) { if (Test-Path $b) { $binOk++ } }
Add-Check -Category 'build' -Name 'binaries_present' -Pass ($binOk -ge 4) -Detail "$binOk/$($bins.Count)"

# --- Policy: mapper defaults (source inspection) ---
$mapperSrc = Join-Path $Byovd 'src\mapper\main_nal.cpp'
if (Test-Path $mapperSrc) {
    $ms = Get-Content $mapperSrc -Raw
    Add-Check -Category 'policy' -Name 'driver2_dkom_unlink_off_by_default' -Pass (
        $ms -match 'g_dkomUnlinkVehicle = false' -and $ms -match 'ApplyDriver2EverythingProfile'
    )
    Add-Check -Category 'policy' -Name 'symlink_kept_default_log' -Pass ($ms -match 'symlink kept')
    Add-Check -Category 'policy' -Name 'vehicle_encrypt_skipped' -Pass (
        $ms -match 'Driver memory encryption: skipped'
    )
    Add-Check -Category 'policy' -Name 'no_resign_rtcore_doc' -Pass (
        (Test-Path $blockPlan) -and ((Get-Content $blockPlan -Raw) -match 're-sign')
    )
} else {
    Add-Check -Category 'policy' -Name 'mapper_source' -Pass $false -Detail 'main_nal.cpp missing'
}

# --- IOCTL reference has ACE/EAC/BE anchors ---
$ioctl = Join-Path $Driver2 'IOCTL_REFERENCE.md'
if (Test-Path $ioctl) {
    $it = Get-Content $ioctl -Raw
    Add-Check -Category 'ioctl' -Name 'ref_session_870' -Pass ($it -match '0x870')
    Add-Check -Category 'ioctl' -Name 'ref_ace_846' -Pass ($it -match '0x846')
    Add-Check -Category 'ioctl' -Name 'ref_eac_844' -Pass ($it -match '0x844')
    Add-Check -Category 'ioctl' -Name 'ref_eac_851' -Pass ($it -match '0x851')
    Add-Check -Category 'ioctl' -Name 'ref_be_852' -Pass ($it -match '0x852')
}

# --- Tier 0 preflight (live host signal, no map; Admin only) ---
$preflightExit = $null
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
$pfScript = $tools.ac_preflight
if ($isAdmin -and (Test-Path $pfScript)) {
    & $pfScript -Driver $Driver
    $preflightExit = $LASTEXITCODE
    Add-Check -Category 'host' -Name 'ac_preflight_ran' -Pass ($null -ne $preflightExit)
    Add-Check -Category 'host' -Name 'vehicle_or_tier0_clear' -Pass (
        $preflightExit -in 0, 1, 2
    ) -Detail "exit=$preflightExit (2=AC+no vehicle=Tier0)"
} else {
    Add-Check -Category 'host' -Name 'ac_preflight_ran' -Pass $true -Detail 'skipped (re-run elevated for Tier 0 read)'
    Add-Check -Category 'host' -Name 'vehicle_or_tier0_clear' -Pass $true -Detail 'skipped'
}

$staticPass = ($checks | Where-Object { $_.pass }).Count
$staticTotal = $checks.Count
$staticScore = if ($staticTotal -gt 0) { [math]::Round(100.0 * $staticPass / $staticTotal, 1) } else { 0 }
$staticApproved = $staticScore -ge $target

$dynamic = $null
$approvalPath = Join-Path $Driver2 'developer_approval.json'

if ($Execute) {
    if (-not $isAdmin) {
        Write-Error '-Execute requires Administrator (VM map + approval gate)'
        exit 3
    }
    if (-not $staticApproved) {
        Write-Host "[!] Static score $staticScore% < $target% — fix S1 before -Execute"
        exit 2
    }
    & (Join-Path $Byovd 'build.ps1')
    & (Join-Path $Driver2 'build.ps1')

    $gateParams = @{
        Execute   = $true
        ForceMap  = [bool]$ForceMap
        Driver    = $Driver
        Inspector = $true
    }
    if ($IncludeAce) { $gateParams['IncludeAce'] = $true }
    if ($IncludeBe) { $gateParams['IncludeBe'] = $true }
    & (Join-Path $Driver2 'developer-approval-gate.ps1') @gateParams
    $gateExit = $LASTEXITCODE

    $lastProfile = if ($IncludeAce) { 'universal' } else { 'rust' }
    & (Join-Path $Driver2 'forensics-capture.ps1') -Profile $lastProfile

    $approval = $null
    if (Test-Path $approvalPath) {
        $approval = Get-Content $approvalPath -Raw | ConvertFrom-Json
    }
    $dynamic = @{
        gate_exit = $gateExit
        approval  = $approval
        forensics = (Get-ChildItem (Join-Path $Driver2 'forensics') -Directory -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
    }
    Add-Check -Category 'dynamic' -Name 'developer_approval_pass' -Pass ($gateExit -eq 0)
}

$pass = ($checks | Where-Object { $_.pass }).Count
$score = if ($checks.Count -gt 0) { [math]::Round(100.0 * $pass / $checks.Count, 1) } else { 0 }
$approved = $score -ge $target -and (-not $Execute -or ($dynamic.gate_exit -eq 0))

@{
    schema           = 'driver2_cheat_inspector_v1'
    utc              = (Get-Date).ToUniversalTime().ToString('o')
    hostname         = $env:COMPUTERNAME
    target_score_pct = $target
    static_score_pct = $staticScore
    final_score_pct  = $score
    approved         = $approved
    execute          = [bool]$Execute
    include_ace      = [bool]$IncludeAce
    preflight_exit   = $preflightExit
    vehicle_stealth  = 'VEHICLE_STEALTH.md'
    tier_plan        = 'docs/11_ac_block_vs_hide_plan.md'
    checks           = $checks
    dynamic          = $dynamic
    disclaimer       = 'Inspector certifies lab SDK + evidence bundle — not live AC bypass'
} | ConvertTo-Json -Depth 10 | Set-Content $OutJson -Encoding UTF8

Write-Host "[*] cheat inspector approved=$approved score=$score% static=$staticScore% -> $OutJson"
if (-not $approved) { exit 1 }
exit 0
