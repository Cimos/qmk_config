# Update the half plugged into this PC: back up VIA, enter the bootloader,
# copy the UF2, then restore and check the keymap. No typing needed.
# Usage: powershell -ExecutionPolicy Bypass -File update.ps1 -Uf2 <file.uf2>
param([Parameter(Mandatory)][string]$Uf2, [string]$Python = "$PSScriptRoot\venv\Scripts\python.exe")
$ErrorActionPreference = 'Stop'
$via = "$PSScriptRoot\via.py"
$backup = "$PSScriptRoot\backup-$(Get-Date -Format yyyyMMdd-HHmmss).json"
& $Python $via backup $backup; if ($LASTEXITCODE) { throw "backup failed" }
& $Python $via bootloader
Write-Output "waiting for RPI-RP2 drive (press BOOT if the firmware is older than 2026-11)"
$deadline = (Get-Date).AddMinutes(10)
do { $v = Get-Volume | Where-Object FileSystemLabel -eq 'RPI-RP2' | Select-Object -First 1; Start-Sleep -Milliseconds 500 }
until ($v -or (Get-Date) -gt $deadline)
if (-not $v) { throw "no bootloader drive" }
Copy-Item $Uf2 "$($v.DriveLetter):\" -ErrorAction SilentlyContinue
Start-Sleep 3
for ($i = 0; $i -lt 60; $i++) { & $Python $via show 2>$null; if ($LASTEXITCODE -eq 0) { break }; Start-Sleep 1 }
& $Python $via restore $backup
