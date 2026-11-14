# Scheduled task target (HIGHEST). Host queues driver2\vm_lab_request.json then schtasks /Run Driver2VmLabWorker
$env:DRIVER2_VM_LAB_WORKER = '1'
$here = if ($PSScriptRoot) { $PSScriptRoot } else { 'Z:\byovd\driver2' }
& (Join-Path $here 'vm-lab.ps1')
exit $LASTEXITCODE
