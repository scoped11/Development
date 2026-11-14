# Back-compat wrapper — EAC only (prefer sync-ac-dumps.ps1).
& (Join-Path $PSScriptRoot 'sync-ac-dumps.ps1') -Family eac
