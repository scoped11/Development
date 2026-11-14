# Patch Driver2LabAttest.inf ManufacturerName from enrollment.json (run after you set legal_name / inf_manufacturer).
param(
    [string]$Enrollment = (Join-Path $PSScriptRoot 'enrollment.json'),
    [string]$InfPath = (Join-Path $PSScriptRoot 'Driver2LabAttest.inf')
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path $Enrollment)) {
    Write-Warning "Missing $Enrollment — copy enrollment.template.json first"
    exit 1
}
$e = Get-Content $Enrollment -Raw | ConvertFrom-Json
$name = if ($e.inf_manufacturer) { $e.inf_manufacturer } elseif ($e.legal_name) { $e.legal_name } else {
    throw 'Set legal_name or inf_manufacturer in enrollment.json'
}
$text = Get-Content $InfPath -Raw
$text = $text -replace 'ManufacturerName="[^"]*"', "ManufacturerName=`"$name`""
Set-Content -Path $InfPath -Value $text -Encoding UTF8 -NoNewline
Write-Host "[+] ManufacturerName=$name"