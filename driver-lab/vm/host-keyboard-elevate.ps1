# Headless: Win+R -> command -> Ctrl+Shift+Enter -> UAC Yes. No host window / no RDP.
param(
    [string]$VmName = 'DriverLab-E-26200',
    [string]$GuestCommand = 'Z:\driver-lab\vm\bg\install.cmd'
)
$ErrorActionPreference = 'SilentlyContinue'
$VBox = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
function K([string[]]$c) {
    & $VBox controlvm $VmName keyboardputscancode @c | Out-Null
    Start-Sleep -Milliseconds 100
}
function Shot($n) { & $VBox controlvm $VmName screenshotpng (Join-Path $env:TEMP $n) | Out-Null }

Write-Host '[*] Win+R'
K @('e0','5b'); Start-Sleep -Milliseconds 200
K @('13'); K @('93'); K @('e0','db')
Start-Sleep -Milliseconds 900
Shot 'dl-r.png'

Write-Host '[*] Ctrl+A + path'
K @('1d'); K @('1e'); K @('9e'); K @('9d')
Start-Sleep -Milliseconds 200
& $VBox controlvm $VmName keyboardputstring $GuestCommand | Out-Null
Start-Sleep -Milliseconds 400
Shot 'dl-t.png'

Write-Host '[*] Ctrl+Shift+Enter'
K @('1d'); K @('2a'); K @('1c'); Start-Sleep -Milliseconds 150
K @('9c'); K @('aa'); K @('9d')
Start-Sleep -Seconds 2
Shot 'dl-u.png'

Write-Host '[*] UAC Left+Enter (Yes)'
K @('e0','4b'); K @('e0','cb'); Start-Sleep -Milliseconds 200
K @('1c'); K @('9c')
Start-Sleep -Seconds 4
Shot 'dl-d.png'
Write-Host '[+] done'
exit 0
