$ErrorActionPreference = "Continue"
Write-Output "GUEST_HOST=$env:COMPUTERNAME"
Write-Output "Z_EXISTS=$(Test-Path 'Z:\')"
if (Test-Path 'Z:\') {
    cmd /c 'dir Z:\'
}
$roots = @(
    'C:\Windows\System32\drivers',
    'C:\Program Files (x86)\Common Files\BattlEye',
    'C:\Program Files (x86)',
    'C:\Program Files',
    'C:\re',
    'C:\Windows\System32\DriverStore',
    'Z:\ac-captures-host'
)
foreach ($r in $roots) {
    Write-Output "ROOT $r exists=$(Test-Path -LiteralPath $r)"
    if (Test-Path -LiteralPath $r) {
        Get-ChildItem -LiteralPath $r -Filter 'BEDaisy.sys' -Recurse -Force -ErrorAction SilentlyContinue |
            Select-Object FullName, Length, LastWriteTime |
            Format-Table -AutoSize |
            Out-String
    }
}
if (Test-Path 'C:\Program Files (x86)\Common Files\BattlEye') {
    Write-Output '=== guest BattlEye dir ==='
    Get-ChildItem 'C:\Program Files (x86)\Common Files\BattlEye' -Force -ErrorAction SilentlyContinue |
        Select-Object Name, Length, LastWriteTime |
        Format-Table -AutoSize |
        Out-String
}
if (Test-Path 'C:\re\artifacts') {
    Write-Output '=== guest C:\re\artifacts ==='
    Get-ChildItem 'C:\re\artifacts' -Force |
        Select-Object Name, Length |
        Format-Table -AutoSize |
        Out-String
}
Get-Service -Name BEDaisy, BEService -ErrorAction SilentlyContinue |
    Format-Table Name, Status, StartType -AutoSize |
    Out-String
Write-Output 'GUEST_HUNT_DONE'
