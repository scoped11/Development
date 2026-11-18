# Run a program inside DriverLab-E-26200 via VirtualBox Guest Additions (host-side).
param(
    [Parameter(Mandatory)][string[]]$Command,
    [string]$VmName = 'AI_workspace',
    [string]$CredentialsFile = '',
    [int]$TimeoutMs = 120000,
    [string]$Cwd = 'C:\\Windows\\System32',
    [hashtable]$PutEnv = @{}
)

$ErrorActionPreference = 'SilentlyContinue'
if (-not $CredentialsFile) {
    $scriptDir = $PSScriptRoot
    if (-not $scriptDir) { $scriptDir = Split-Path -LiteralPath $PSCommandPath -Parent }
    $CredentialsFile = Join-Path $scriptDir 'lab.credentials.json'
}
$VBox = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
if (-not (Test-Path $VBox)) { throw 'VirtualBox not found' }
if (-not (Test-Path $CredentialsFile)) {
    throw "Missing $CredentialsFile — copy lab.credentials.example.json and set password"
}

$cred = Get-Content $CredentialsFile -Raw | ConvertFrom-Json
$exe = $Command[0]
if ($exe -notmatch '[\\/]' -and $exe -notmatch '\.exe$') {
    $exe = Join-Path $env:SystemRoot "System32\$exe.exe"
}
$args = @()
if ($Command.Count -gt 1) { $args = $Command[1..($Command.Count - 1)] }

$vboxArgs = @(
    'guestcontrol', $VmName, 'run',
    "--username=$($cred.username)",
    "--password=$($cred.password)",
    "--exe=$exe",
    '--wait-stdout', '--wait-stderr',
    "--timeout=$TimeoutMs",
    "--cwd=$Cwd"
)
if ($cred.domain) { $vboxArgs += "--domain=$($cred.domain)" }
foreach ($k in $PutEnv.Keys) {
    $vboxArgs += "--putenv=$k=$($PutEnv[$k])"
}
$vboxArgs += '--'
$vboxArgs += $args

& $VBox @vboxArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
