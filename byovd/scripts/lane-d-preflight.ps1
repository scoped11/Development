#Requires -RunAsAdministrator
# Lane D gate: when BYOVD is blocked under AC, recommend split-trust (DMA live + BYOVD lab).
param(
    [string]$Game = "generic"
)

$ErrorActionPreference = "Continue"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$DmaRoot = Join-Path (Split-Path -Parent $Root) "dma"
$Driver3 = Join-Path $Root "driver3"

. (Join-Path $Driver3 "Resolve-Driver3Game.ps1")
$profile = if ($Game -eq "generic") { "generic" } else { Resolve-Driver3Profile $Game }
$meta = $null
if ($profile -ne "generic") { $meta = Get-Driver3Meta $profile }

Write-Host "[*] Lane D — Split-Trust Calibration Bridge"
Write-Host "    doc: docs/12_lane_d_split_trust.md"
if ($meta) {
    Write-Host ("    title: {0} ({1})" -f $meta.title, $meta.ac_family)
}
Write-Host ""

& (Join-Path $Root "scripts\ac-preflight.ps1") -Driver rtcore
$acExit = $LASTEXITCODE

$scsiDma = Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Control\DmaSecurity" -ErrorAction SilentlyContinue
Write-Host "[*] Kernel DMA Protection / DMA policy key present: $(if ($scsiDma) { 'yes' } else { 'unknown / not set' })"

if (Test-Path $DmaRoot) {
    Write-Host "[+] DMA tree: $DmaRoot"
} else {
    Write-Host "[!] DMA tree not found at $DmaRoot — live lane unavailable"
}

if ($meta) {
    $live = Test-Driver3LiveAcForTitle $profile
    if ($live) {
        $procs = Get-Driver3AcProcesses -Profile $profile
        Write-Host ("[*] Live usermode AC for {0}: {1}" -f $profile, ($procs.ProcessName -join ", "))
    }
    Write-Host ("[*] VM proof: driver3\host-b.ps1 -Profile {0} -Execute -ForceMap" -f $profile)
    Write-Host ("    driver2: {0}" -f $meta.driver2_profile_file)
    if ($meta.capture_dump) {
        Write-Host ("    dumps: {0}" -f $meta.capture_dump)
    }
    switch ($meta.ac_family) {
        "eac"       { Write-Host "    IOCTL lab: EAC 0x844/0x851 (profile eac-rust)" }
        "battleye"  { Write-Host "    IOCTL lab: BE 0x852 (profile be-r6 / be-arksa)" }
        "ace"       { Write-Host "    IOCTL lab: ACE 0x846/0x84E/0x86A (ace-deltaforce / ace-arenabreakout)" }
    }
} elseif ($Game -eq "generic") {
    Write-Host "[*] Pass -Game rust|r6|deltaforce|arenabreakout|arksa (or alias — see driver3/games.json)"
}

Write-Host ""
if ($acExit -eq 2) {
    Write-Host "[+] Lane D recommended:"
    Write-Host "    Host A (this PC, AC on): DMA reads — dma\scripts\build-windows.ps1"
    Write-Host "    Host B (VM, no AC):      driver3\host-b.ps1 -Profile" $(if ($meta) { $profile } else { "<game>" }) "-Execute -ForceMap"
    Write-Host "    Sync:                    docs/lab_manifest.json lane_d.build_id + by_profile"
    exit 2
}
if ($acExit -eq 1) {
    Write-Host "[*] Vehicle not open — copy RTCore to bin\ or start service; then Tier 1 or Lane D Host B"
    exit 1
}
Write-Host "[+] BYOVD preflight OK on this host; Lane D optional unless live paid-AC session here"
exit 0
