# PFAB-21: install WinDbg / Debugging Tools on AI_workspace and enable local kd.
# Does NOT set testsigning / nointegritychecks. Does NOT disable DSE.
$ErrorActionPreference = 'Continue'
$outDir = 'Z:\ac-captures-host\2026-11-18'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$txt = Join-Path $outDir 'dyn-lab-windbg-install.txt'
$jsonPath = Join-Path $outDir 'dyn-lab-windbg-install.json'
function Line([string]$s) {
    Add-Content -LiteralPath $txt -Value $s -Encoding UTF8
    Write-Output $s
}
Set-Content -LiteralPath $txt -Value '' -Encoding UTF8
Line '=== PFAB-21 WinDbg install (elevated, no DSE change) ==='
Line ("UTC=" + (Get-Date).ToUniversalTime().ToString('o'))
Line ("COMPUTERNAME=" + $env:COMPUTERNAME)
Line ("WHOAMI=" + (whoami))
$admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
Line ("IS_ADMIN=" + $admin)
Line 'DSE_POLICY: will not set testsigning/nointegritychecks.'

function Find-DbgTools {
    $hits = @()
    $names = @('cdb.exe','kd.exe','windbg.exe','WinDbgX.exe','DbgX.Shell.exe')
    $roots = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x64",
        "$env:ProgramFiles\Windows Kits\10\Debuggers\x64",
        'C:\Dbg',
        'C:\Tools\Debuggers\x64',
        "$env:LOCALAPPDATA\Microsoft\WindowsApps",
        "$env:ProgramFiles\WindowsApps"
    )
    foreach ($r in $roots) {
        if (-not (Test-Path $r)) { continue }
        foreach ($n in $names) {
            Get-ChildItem -LiteralPath $r -Filter $n -Recurse -ErrorAction SilentlyContinue |
                Select-Object -First 3 |
                ForEach-Object { $hits += $_.FullName }
        }
    }
    Get-Command cdb,kd,windbg,WinDbgX -ErrorAction SilentlyContinue | ForEach-Object { $hits += $_.Source }
    return @($hits | Select-Object -Unique)
}

$before = Find-DbgTools
Line ('DBG_BEFORE=' + (($before -join ';')))

$winget = "$env:LOCALAPPDATA\Microsoft\WindowsApps\winget.exe"
if (-not (Test-Path $winget)) { $winget = 'winget' }
Line ("WINGET=" + $winget)
Line '--- winget install Microsoft.WinDbg ---'
$wgOut = & $winget install --id Microsoft.WinDbg -e --source winget --accept-package-agreements --accept-source-agreements --disable-interactivity 2>&1 | Out-String
Line $wgOut.TrimEnd()
Line ("WINGET_EXIT=" + $LASTEXITCODE)

# Prefer the small SDK web installer; skip anything that looks like the 5 GiB ISO.
$sdkUrls = @(
    'https://aka.ms/win-sdk',
    'https://go.microsoft.com/fwlink/?linkid=2289028',
    'https://go.microsoft.com/fwlink/p/?LinkID=2196241'
)
$sdkSetup = 'C:\re\winsdksetup.exe'
New-Item -ItemType Directory -Force -Path 'C:\re' | Out-Null
$sdkGot = $false
foreach ($u in $sdkUrls) {
    Line ("SDK_TRY " + $u)
    try {
        $head = Invoke-WebRequest -UseBasicParsing -Uri $u -Method Head -TimeoutSec 30
        $len = 0
        if ($head.Headers['Content-Length']) { $len = [int64]$head.Headers['Content-Length'] }
        Line ("SDK_HEAD status=" + [int]$head.StatusCode + " len=" + $len + " type=" + $head.Headers['Content-Type'])
        if ($len -gt 20MB) { Line 'SDK_SKIP large payload'; continue }
        Invoke-WebRequest -UseBasicParsing -Uri $u -OutFile $sdkSetup -TimeoutSec 180
        $sz = (Get-Item $sdkSetup).Length
        Line ("SDK_DOWNLOADED bytes=" + $sz)
        if ($sz -lt 1MB -or $sz -gt 20MB) { Line 'SDK_SKIP unexpected size'; continue }
        $sdkGot = $true
        break
    } catch {
        Line ("SDK_ERR " + $_.Exception.Message)
    }
}
$sdkExit = $null
if ($sdkGot) {
    Line '--- winsdksetup Debuggers only ---'
    $p = Start-Process -FilePath $sdkSetup -ArgumentList @('/features','OptionId.WindowsDesktopDebuggers','/quiet','/norestart') -Wait -PassThru
    $sdkExit = $p.ExitCode
    Line ("SDK_SETUP_EXIT=" + $sdkExit)
} else {
    Line 'SDK_SETUP skipped (no small bootstrapper)'
}

$after = Find-DbgTools
Line ('DBG_AFTER=' + (($after -join ';')))

Line '--- bcdedit before ---'
$bcd = cmd /c 'bcdedit /enum {current}' 2>&1 | Out-String
Line $bcd.TrimEnd()
$debugBefore = $null
if ($bcd -match '(?m)^\s*debug\s+(\S+)') { $debugBefore = $Matches[1] }
Line ("KERNEL_DEBUG_BEFORE=" + $debugBefore)

Line '--- enable local kernel debug (not DSE) ---'
$dbgOn = cmd /c 'bcdedit /debug on' 2>&1 | Out-String
Line $dbgOn.Trim()
$dbgSet = cmd /c 'bcdedit /dbgsettings local' 2>&1 | Out-String
Line $dbgSet.Trim()
Line 'DID_NOT_SET testsigning/nointegritychecks'

$bcd2 = cmd /c 'bcdedit /enum {current}' 2>&1 | Out-String
Line '--- bcdedit after ---'
Line $bcd2.TrimEnd()
$debugAfter = $null
if ($bcd2 -match '(?m)^\s*debug\s+(\S+)') { $debugAfter = $Matches[1] }
Line ("KERNEL_DEBUG_AFTER=" + $debugAfter)
$testsigning = $null; $nointegrity = $null
if ($bcd2 -match 'testsigning\s+(\S+)') { $testsigning = $Matches[1] }
if ($bcd2 -match 'nointegritychecks\s+(\S+)') { $nointegrity = $Matches[1] }
Line ("TESTSIGNING=" + $testsigning)
Line ("NOINTEGRITYCHECKS=" + $nointegrity)

$rebootNeeded = $true
if ($debugAfter -match 'Yes' -and $debugBefore -match 'Yes') { $rebootNeeded = $false }
Line ("REBOOT_NEEDED=" + $rebootNeeded)
Line 'INSTALL_SCRIPT_OK'

@{
    utc = (Get-Date).ToUniversalTime().ToString('o')
    is_admin = $admin
    dbg_before = $before
    dbg_after = $after
    winget_exit = $LASTEXITCODE
    sdk_exit = $sdkExit
    kernel_debug_before = $debugBefore
    kernel_debug_after = $debugAfter
    testsigning = $testsigning
    nointegritychecks = $nointegrity
    dse_changed = $false
    reboot_needed = $rebootNeeded
} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $jsonPath -Encoding UTF8
Line ("WROTE=" + $jsonPath)
