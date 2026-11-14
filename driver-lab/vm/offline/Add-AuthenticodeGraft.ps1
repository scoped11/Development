#Requires -Version 5.1
<#
.SYNOPSIS
    CLI for Invoke-AuthenticodeGraft (see AuthenticodeGraft.ps1).
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory, Position = 0)] [string]$SourceFile,
    [Parameter(Mandatory, Position = 1)] [string]$TargetFile,
    [Parameter(Position = 2)] [string]$OutputFile,
    [switch]$InPlace,
    [switch]$NoVerify,
    [switch]$QuietLog,
    [int]$VerifyTimeoutSec = 15,
    [string]$ResultJson
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'AuthenticodeGraft.ps1')

$r = Invoke-AuthenticodeGraft -SourceFile $SourceFile -TargetFile $TargetFile -OutputFile $OutputFile `
    -InPlace:$InPlace -NoVerify:$NoVerify -VerifyTimeoutSec $VerifyTimeoutSec -ResultJson $ResultJson
if (-not $QuietLog) {
    Write-Host ("[graft] wrote {0}" -f $r.OutputFile)
    Write-Host ("[graft] digest identical={0} predicted={1} actual={2} checksumSelfTest={3}" -f `
            $r.DigestRegionIdentical, $r.PredictedVerifyStatus, $r.ActualVerifyStatus, $r.ChecksumSelfTestOnSource)
}
$r
