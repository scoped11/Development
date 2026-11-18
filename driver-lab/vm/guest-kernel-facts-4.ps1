# Static kernel facts pass 4. Drivers NOT loaded. BEDaisy: skip auto-analyze.
$ErrorActionPreference = "Continue"
$env:GHIDRA_INSTALL_DIR = "C:\Tools\ghidra_12.1_PUBLIC"
$env:JAVA_HOME = "C:\Program Files\Microsoft\jdk-21.0.13.21-hotspot"
$env:Path = "C:\Program Files\Microsoft\jdk-21.0.13.21-hotspot\bin;C:\Program Files\nodejs;$env:Path"
$out = "Z:\ac-captures-host\2026-11-18"
$art = "C:\re\artifacts"
$projRoot = "C:\re\ghidra-projects"
$headless = "C:\Tools\ghidra_12.1_PUBLIC\support\analyzeHeadless.bat"
New-Item -ItemType Directory -Force -Path $out, $art, $projRoot | Out-Null

function Invoke-Headless {
    param([string]$LogName, [string[]]$Args)
    $log = Join-Path $out $LogName
    Write-Output "=== HEADLESS $LogName $(Get-Date -Format o) ==="
    & $headless @Args 2>&1 | Tee-Object $log
    Write-Output "=== EXIT $LogName code=$LASTEXITCODE $(Get-Date -Format o) ==="
}

Write-Output "STAGE $(Get-Date -Format o)"
foreach ($pair in @(
    @("Z:\ac-captures-host\2026-11-17\EasyAntiCheat_EOS.sys", "$art\EasyAntiCheat_EOS.sys"),
    @("Z:\ac-captures-host\2026-11-17\EasyAntiCheat.sys", "$art\EasyAntiCheat.sys"),
    @("Z:\ac-captures-host\2026-11-18\BEDaisy.sys", "$art\BEDaisy.sys")
)) {
    if (Test-Path $pair[0]) { Copy-Item $pair[0] $pair[1] -Force }
    Write-Output ("HASH " + [IO.Path]::GetFileName($pair[1]) + " " + (Get-FileHash $pair[1] -Algorithm SHA256).Hash)
}

# --- BEDaisy: import WITHOUT analysis, then limited AddressSet disasm ---
Write-Output "BEDAISY_NOANALYSIS $(Get-Date -Format o)"
Invoke-Headless "ghidra-bedaisy-noanalysis.log" @(
    $projRoot, "BEDaisyNoA",
    "-import", "$art\BEDaisy.sys",
    "-overwrite",
    "-noanalysis",
    "-scriptPath", "Z:\driver-lab\vm",
    "-postScript", "DumpLimitedDisasm.py"
)

# --- EAC EOS: persistent project (auto-analyze once), then dump selected funcs ---
Write-Output "EAC_EOS_PROJECT $(Get-Date -Format o)"
$eosRep = Join-Path $projRoot "EAC_EOS.rep"
if (-not (Test-Path $eosRep)) {
    Invoke-Headless "ghidra-eos-import.log" @(
        $projRoot, "EAC_EOS",
        "-import", "$art\EasyAntiCheat_EOS.sys",
        "-overwrite"
    )
} else {
    Write-Output "EAC_EOS project exists, skip import"
}
if (Test-Path $eosRep) {
    Invoke-Headless "ghidra-eos-dump.log" @(
        $projRoot, "EAC_EOS",
        "-process", "EasyAntiCheat_EOS.sys",
        "-noanalysis",
        "-scriptPath", "Z:\driver-lab\vm",
        "-postScript", "DumpEacFunctions.py"
    )
}

# --- EAC legacy ---
Write-Output "EAC_LEGACY_PROJECT $(Get-Date -Format o)"
$legRep = Join-Path $projRoot "EAC_LEGACY.rep"
if (-not (Test-Path $legRep)) {
    Invoke-Headless "ghidra-legacy-import.log" @(
        $projRoot, "EAC_LEGACY",
        "-import", "$art\EasyAntiCheat.sys",
        "-overwrite"
    )
} else {
    Write-Output "EAC_LEGACY project exists, skip import"
}
if (Test-Path $legRep) {
    Invoke-Headless "ghidra-legacy-dump.log" @(
        $projRoot, "EAC_LEGACY",
        "-process", "EasyAntiCheat.sys",
        "-noanalysis",
        "-scriptPath", "Z:\driver-lab\vm",
        "-postScript", "DumpEacFunctions.py"
    )
}

Write-Output "GUEST_KERNEL_FACTS_4_DONE $(Get-Date -Format o)"
