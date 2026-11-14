# Default repo to driver2 ranked (single PC) for Rust/R6.
& (Join-Path (Split-Path $PSScriptRoot -Parent) 'driver3\set-live-read-method.ps1') -Method single_pc_kernel -Game rust
& (Join-Path $PSScriptRoot 'ranked-ready.ps1') -PreflightOnly
Write-Host '[+] Default live_read_method=single_pc_kernel'
Write-Host '    Ranked-safe play: .\driver3\single-pc-play.ps1 -Game rust|r6 -Execute -Live'
Write-Host '    100% verify (admin): .\driver2\confirm-ranked-100.ps1 -Title both'
Write-Host '    Or live map: .\driver2\ranked-ready.ps1 -Execute -Live'
