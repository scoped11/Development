# BEDaisy Ghidra import with -noanalysis. Do NOT auto-analyze (CreateThunkFunctionCmd SOE).
$ErrorActionPreference = "Continue"
$env:GHIDRA_INSTALL_DIR = "C:\Tools\ghidra_12.1_PUBLIC"
$env:JAVA_HOME = "C:\Program Files\Microsoft\jdk-21.0.13.21-hotspot"
$env:Path = "C:\Program Files\Microsoft\jdk-21.0.13.21-hotspot\bin;$env:Path"
$out = "Z:\ac-captures-host\2026-11-18"
$art = "C:\re\artifacts"
$projRoot = "C:\re\ghidra-projects"
$headless = "C:\Tools\ghidra_12.1_PUBLIC\support\analyzeHeadless.bat"
New-Item -ItemType Directory -Force -Path $out, $art, $projRoot | Out-Null
Write-Output "BEDAISY_NOANALYSIS_START $(Get-Date -Format o)"
Write-Output ("HASH " + (Get-FileHash "$art\BEDaisy.sys" -Algorithm SHA256).Hash)
& $headless $projRoot "BEDaisyNoA" -import "$art\BEDaisy.sys" -overwrite -noanalysis -scriptPath "Z:\driver-lab\vm" -postScript "DumpLimitedDisasm.py" 2>&1 |
    Tee-Object "$out\ghidra-bedaisy-noanalysis.log"
Write-Output "EXIT=$LASTEXITCODE $(Get-Date -Format o)"
if (Test-Path "$art\bedaisy-limited-disasm.txt") {
    Copy-Item "$art\bedaisy-limited-disasm.txt" "$out\bedaisy-limited-disasm.txt" -Force
    Write-Output "COPIED limited disasm"
}
Write-Output "BEDAISY_NOANALYSIS_DONE $(Get-Date -Format o)"
