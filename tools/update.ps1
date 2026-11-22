# Update the MadMan Corne from Windows. No typing needed.
#   update.ps1 -Uf2 <file.uf2>         update the half plugged in now
#   update.ps1 -Uf2 <file.uf2> -Both   then wait for the cable swap and do the other half
# Each half: back up VIA, reboot into the bootloader, copy the UF2, restore and check.
# A half that is already in boot mode is flashed and restored from the latest backup.
param([Parameter(Mandatory)][string]$Uf2, [switch]$Both,
      [string]$Python)
$ErrorActionPreference = 'Stop'
if (-not $Python) { $Python = Join-Path $PSScriptRoot 'venv\Scripts\python.exe' }  # PS 5.1 has no $PSScriptRoot in param defaults
$via = "$PSScriptRoot\via.py"

function Get-Drive { Get-Volume | Where-Object FileSystemLabel -eq 'RPI-RP2' | Select-Object -First 1 }
function Get-Serial {
    $d = Get-PnpDevice -PresentOnly -Class USB -ErrorAction SilentlyContinue |
         Where-Object InstanceId -match '^USB\\VID_4653&PID_0002\\' | Select-Object -First 1
    if ($d) { ($d.InstanceId -split '\\')[-1].Substring(0, 16) }
}
function Wait-For([scriptblock]$Test, [int]$Minutes, [string]$What) {
    $deadline = (Get-Date).AddMinutes($Minutes)
    while (-not ($r = & $Test)) {
        if ((Get-Date) -gt $deadline) { throw "timed out waiting for $What" }
        Start-Sleep -Milliseconds 500
    }
    $r
}

function Update-Half {
    if (Get-Drive) {
        $backup = Get-ChildItem "$PSScriptRoot\backup-*.json" | Sort-Object LastWriteTime | Select-Object -Last 1
        if (-not $backup) { throw "half is in boot mode and there is no backup to restore" }
        $backup = $backup.FullName
        Write-Output "already in boot mode, will restore from $(Split-Path $backup -Leaf)"
    } else {
        $backup = "$PSScriptRoot\backup-$(Get-Date -Format yyyyMMdd-HHmmss).json"
        & $Python $via backup $backup; if ($LASTEXITCODE) { throw "backup failed" }
        & $Python $via bootloader
        Write-Output "waiting for the RPI-RP2 drive (press BOOT on firmware older than 2026-11)"
    }
    $v = Wait-For { Get-Drive } 10 'the RPI-RP2 drive'
    Copy-Item $Uf2 "$($v.DriveLetter):\" -ErrorAction SilentlyContinue
    Start-Sleep 3
    $null = Wait-For { $ErrorActionPreference = 'Continue'; & $Python $via show 2>$null; $LASTEXITCODE -eq 0 } 2 'the keyboard to come back'
    & $Python $via restore $backup
    if ($LASTEXITCODE) { throw "restore failed" }
    Get-Serial
}

$first = Update-Half | Tee-Object -Variable out | Select-Object -Last 1
$out | Select-Object -SkipLast 1
Write-Output "done: half $first"
if ($Both) {
    Write-Output "move the cable to the other half"
    $null = Wait-For { (Get-Drive) -or ((Get-Serial) -and (Get-Serial) -ne $first) } 45 'the other half'
    Start-Sleep 3
    $second = Update-Half | Tee-Object -Variable out | Select-Object -Last 1
    $out | Select-Object -SkipLast 1
    Write-Output "done: half $second"
}
