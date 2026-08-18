param(
    [string]$teensyDir = "$env:LOCALAPPDATA\Arduino15\packages\teensy\hardware\avr\1.62.0",
    [string]$backupDir = "$PSScriptRoot\..\backups\1.62.0"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $backupDir)) {
    Write-Error "Backup not found at: $backupDir. Run backup_teensy_audio.ps1 first."
    exit 1
}

Write-Host "Restoring Teensy audio files from: $backupDir"

$backupFiles = Get-ChildItem -LiteralPath $backupDir -File -Recurse | Where-Object {
    $_.FullName -notmatch '\.bak\\'
}
if ($backupFiles.Count -eq 0) {
    Write-Warning "No backup files found."
    exit 0
}

foreach ($f in $backupFiles) {
    $rel = $f.FullName.Substring($backupDir.Length).TrimStart('\')
    $dest = "$teensyDir\$rel"
    $destDir = Split-Path -Parent $dest
    New-Item -ItemType Directory -Force -Path $destDir | Out-Null
    Copy-Item -LiteralPath $f.FullName -Destination $dest -Force
    Write-Host "  Restored: $rel"
}

$cacheDir = "$env:APPDATA\arduino-ide"
if (Test-Path -LiteralPath $cacheDir) {
    Write-Host "`nClearing Arduino IDE 2.x cache..."
    Remove-Item -LiteralPath "$cacheDir\*" -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host "Restore complete."
