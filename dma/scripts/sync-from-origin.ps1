# Pull latest kyle-games/robrun from Cursor Origin (requires origin auth).
# Browse: https://cursor.com/codebase/kyle-games/robrun
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

Write-Host "Fetching origin/main ..."
git pull origin main
Write-Host "HEAD:" (git log -1 --oneline)
