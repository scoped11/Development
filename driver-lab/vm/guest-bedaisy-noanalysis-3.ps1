# Re-import BEDaisy with -noanalysis and Java post-script. Do not auto-analyze.
$ErrorActionPreference = "Continue"
$env:GHIDRA_INSTALL_DIR = "C:\Tools\ghidra_12.1_PUBLIC"
$env:JAVA_HOME = "C:\Program Files\Microsoft\jdk-21.0.13.21-hotspot"
$env:Path = "C:\Program Files\Microsoft\jdk-21.0.13.21-hotspot\bin;$env:Path"
$out = "Z:\ac-captures-host\2026-11-18"
$art = "C:\re\artifacts"
$projRoot = "C:\re\ghidra-projects"
$headless = "C:\Tools\ghidra_12.1_PUBLIC\support\analyzeHeadless.bat"
Write-Output "BEDAISY_DUMP3_START $(Get-Date -Format o)"
Get-ChildItem $projRoot -Recurse -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -match 'sys|gpr|rep|xml' } |
    Select-Object -First 40 FullName, Length |
    Format-Table -AutoSize | Out-String

# Import again: previous run aborted on Jython before a usable -process name.
& $headless $projRoot "BEDaisyNoA" -import "$art\BEDaisy.sys" -overwrite -noanalysis -scriptPath "Z:\driver-lab\vm" -postScript "DumpLimitedDisasm.java" 2>&1 |
    Tee-Object "$out\ghidra-bedaisy-noanalysis-3.log"
Write-Output "EXIT=$LASTEXITCODE $(Get-Date -Format o)"
if (Test-Path "$art\bedaisy-limited-disasm.txt") {
    Write-Output "=== DISASM HEAD ==="
    Get-Content "$art\bedaisy-limited-disasm.txt" -TotalCount 100
}
Write-Output "BEDAISY_DUMP3_DONE $(Get-Date -Format o)"
