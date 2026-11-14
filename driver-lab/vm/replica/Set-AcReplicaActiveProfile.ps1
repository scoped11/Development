# One active 1:1 AC stack per VM session (avoid mixing Delta vs ABI ACE-GAME).
#Requires -RunAsAdministrator
param(
    [ValidateSet('rust', 'r6', 'deltaforce', 'arenabreakout')]
    [string]$Profile,
    [switch]$StartServices
)

$ErrorActionPreference = 'Continue'
$allAce = @('ACE-GAME', 'ACE-BASE', 'ACE-ADVT', 'ACE-CORE', 'EasyAntiCheat_EOS', 'BEDaisy')
foreach ($svc in $allAce) {
    sc.exe query $svc 2>$null | Out-Null
    if ($LASTEXITCODE -eq 0) {
        sc.exe stop $svc 2>$null | Out-Null
    }
}
Get-Process BEService, EasyAntiCheat*, SGuardSvc64, ACE-Service64 -EA SilentlyContinue |
    ForEach-Object { Stop-Process -Id $_.Id -Force -EA SilentlyContinue }

$layouts = Get-Content (Join-Path $PSScriptRoot 'replica-layouts.json') -Raw | ConvertFrom-Json
$lay = $layouts.profiles.$Profile
if (-not $lay) { throw "unknown profile $Profile" }

$load = $StartServices.IsPresent
$um = $StartServices.IsPresent
& (Join-Path $PSScriptRoot 'Install-AcReplicaL2.ps1') -Profile $Profile -LoadKernel:$load -StartUsermode:$um

@{
    schema  = 'ac_replica_active_v1'
    profile = $Profile
    utc     = (Get-Date).ToUniversalTime().ToString('o')
} | ConvertTo-Json | Set-Content (Join-Path (Join-Path $PSScriptRoot '..\bg') 'ac-replica-active.json') -Encoding utf8