Get-CimInstance Win32_Process | Where-Object { $_.Name -match 'powershell|cmd|schtasks|fodhelper' } |
    Select-Object ProcessId, Name, CommandLine |
    Format-List
Test-Path C:\DriverLab\e-cmd-done.txt
schtasks /Query /TN Driver3ElevatedWorker /FO LIST
