# EOS/legacy: import WITH analysis once into persistent projects, then -noanalysis decompile dumps.
# Drivers are NOT loaded.
$ErrorActionPreference = "Continue"
$env:GHIDRA_INSTALL_DIR = "C:\Tools\ghidra_12.1_PUBLIC"
$env:JAVA_HOME = "C:\Program Files\Microsoft\jdk-21.0.13.21-hotspot"
$env:Path = "C:\Program Files\Microsoft\jdk-21.0.13.21-hotspot\bin;$env:Path"
$out = "Z:\ac-captures-host\2026-11-18"
$art = "C:\re\artifacts"
$projRoot = "C:\re\ghidra-projects"
$headless = "C:\Tools\ghidra_12.1_PUBLIC\support\analyzeHeadless.bat"
New-Item -ItemType Directory -Force -Path $out, $art, $projRoot | Out-Null

function Invoke-Headless {
    param([string]$LogName, [string[]]$Args)
    Write-Output "=== HEADLESS $LogName $(Get-Date -Format o) ==="
    & $headless @Args 2>&1 | Tee-Object (Join-Path $out $LogName)
    Write-Output "=== EXIT $LogName code=$LASTEXITCODE $(Get-Date -Format o) ==="
}

Write-Output ("EOS " + (Get-FileHash "$art\EasyAntiCheat_EOS.sys" -Algorithm SHA256).Hash)
Write-Output ("LEG " + (Get-FileHash "$art\EasyAntiCheat.sys" -Algorithm SHA256).Hash)

if (-not (Test-Path "$projRoot\EAC_EOS.rep")) {
    Invoke-Headless "ghidra-eos-import.log" @($projRoot, "EAC_EOS", "-import", "$art\EasyAntiCheat_EOS.sys", "-overwrite")
}
if (Test-Path "$projRoot\EAC_EOS.rep") {
    Invoke-Headless "ghidra-eos-dump.log" @(
        $projRoot, "EAC_EOS", "-process", "EasyAntiCheat_EOS.sys", "-noanalysis",
        "-scriptPath", "Z:\driver-lab\vm", "-postScript", "DumpEacFunctions.py"
    )
}
if (-not (Test-Path "$projRoot\EAC_LEGACY.rep")) {
    Invoke-Headless "ghidra-legacy-import.log" @($projRoot, "EAC_LEGACY", "-import", "$art\EasyAntiCheat.sys", "-overwrite")
}
if (Test-Path "$projRoot\EAC_LEGACY.rep") {
    Invoke-Headless "ghidra-legacy-dump.log" @(
        $projRoot, "EAC_LEGACY", "-process", "EasyAntiCheat.sys", "-noanalysis",
        "-scriptPath", "Z:\driver-lab\vm", "-postScript", "DumpEacFunctions.py"
    )
}
Write-Output "EAC_FLT_DUMP_DONE $(Get-Date -Format o)"
