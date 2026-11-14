# Host: run script block in guest via WinRM (127.0.0.1:55985).
param(
    [Parameter(Mandatory)][scriptblock]$ScriptBlock,
    [string]$VmName = 'DriverLab-E-26200',
    [string]$CredentialsFile = '',
    [int]$Port = 55985
)

$ErrorActionPreference = 'Stop'
$Repo = Resolve-Path (Join-Path $PSScriptRoot '..\..')
if (-not $CredentialsFile) {
    $CredentialsFile = Join-Path $Repo 'driver-lab\vm\lab.credentials.json'
}
$enable = Join-Path $Repo 'driver-lab\vm\enable-vm-access.ps1'
if (Test-Path $enable) { & $enable -VmName $VmName | Out-Null }

$cred = Get-Content $CredentialsFile -Raw | ConvertFrom-Json
$sec = ConvertTo-SecureString $cred.password -AsPlainText -Force
$psc = New-Object PSCredential ($cred.username, $sec)
$so = New-PSSessionOption -SkipCACheck -SkipCNCheck -OpenTimeout 60000
$sess = $null
try {
    $sess = New-PSSession -ComputerName '127.0.0.1' -Port $Port -Credential $psc -SessionOption $so
    Invoke-Command -Session $sess -ScriptBlock $ScriptBlock
} finally {
    if ($sess) { Remove-PSSession $sess -ErrorAction SilentlyContinue }
}
