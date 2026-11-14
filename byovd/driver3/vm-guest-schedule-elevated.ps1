# Guest: register + run one-shot elevated task (DriverLab credentials in task).
param(
    [Parameter(Mandatory)][string]$TrCommand,
    [string]$TaskName = 'Driver3ElevatedOnce'
)

$ErrorActionPreference = 'Stop'
schtasks /Delete /TN $TaskName /F 2>$null | Out-Null
$null = schtasks /Create /TN $TaskName /TR $TrCommand /SC ONCE /ST 23:59 /RL HIGHEST /F
if ($LASTEXITCODE -ne 0) { throw "schtasks /Create failed $LASTEXITCODE" }
schtasks /Run /TN $TaskName
if ($LASTEXITCODE -ne 0) { throw "schtasks /Run failed $LASTEXITCODE" }
Write-Host '[+] Elevated task started:' $TaskName
exit 0
