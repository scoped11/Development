# Re-run BEDaisyNoA project with Java DumpLimitedDisasm. No auto-analyze.
$ErrorActionPreference = "Continue"
$env:GHIDRA_INSTALL_DIR = "C:\Tools\ghidra_12.1_PUBLIC"
$env:JAVA_HOME = "C:\Program Files\Microsoft\jdk-21.0.13.21-hotspot"
$env:Path = "C:\Program Files\Microsoft\jdk-21.0.13.21-hotspot\bin;$env:Path"
$out = "Z:\ac-captures-host\2026-11-18"
$projRoot = "C:\re\ghidra-projects"
$headless = "C:\Tools\ghidra_12.1_PUBLIC\support\analyzeHeadless.bat"
Write-Output "BEDAISY_DUMP2_START $(Get-Date -Format o)"
Write-Output "PROJECT=$(Test-Path "$projRoot\BEDaisyNoA.rep")"
Get-ChildItem $projRoot -ErrorAction SilentlyContinue | Select-Object Name | Format-Table | Out-String
& $headless $projRoot "BEDaisyNoA" -process "BEDaisy.sys" -noanalysis -scriptPath "Z:\driver-lab\vm" -postScript "DumpLimitedDisasm.java" 2>&1 |
    Tee-Object "$out\ghidra-bedaisy-noanalysis-2.log"
Write-Output "EXIT=$LASTEXITCODE $(Get-Date -Format o)"
if (Test-Path "C:\re\artifacts\bedaisy-limited-disasm.txt") {
    Get-Content "C:\re\artifacts\bedaisy-limited-disasm.txt" -TotalCount 80
}
Write-Output "BEDAISY_DUMP2_DONE $(Get-Date -Format o)"
