param(
    [string]$teensyDir = "$env:LOCALAPPDATA\Arduino15\packages\teensy\hardware\avr\1.62.0",
    [string]$backupDir = "$PSScriptRoot\..\backups\1.62.0"
)

$ErrorActionPreference = "Stop"

$coreDir = "$teensyDir\cores\teensy4"
$configDir = $teensyDir
$backupCoreDir = "$backupDir\cores\teensy4"
$backupConfigDir = $backupDir

$files = @(
    "$coreDir\usb_audio.h",
    "$coreDir\usb_audio.cpp",
    "$coreDir\usb_desc.h",
    "$coreDir\usb_desc.c",
    "$coreDir\usb.c",
    "$configDir\platform.txt"
)

Write-Host "Backing up Teensy audio library files to: $backupDir"

$missing = @()
foreach ($f in $files) {
    if (-not (Test-Path -LiteralPath $f)) {
        $missing += $f
    }
}
if ($missing.Count -gt 0) {
    Write-Warning "The following files do not exist (will be skipped):"
    foreach ($m in $missing) { Write-Warning "  $m" }
}

New-Item -ItemType Directory -Force -Path $backupCoreDir | Out-Null
New-Item -ItemType Directory -Force -Path $backupConfigDir | Out-Null

foreach ($f in $files) {
    if (Test-Path -LiteralPath $f) {
        $rel = $f.Substring($teensyDir.Length).TrimStart('\')
        $dest = "$backupDir\$rel"
        $destDir = Split-Path -Parent $dest
        New-Item -ItemType Directory -Force -Path $destDir | Out-Null
        Copy-Item -LiteralPath $f -Destination $dest -Force
        Write-Host "  Saved: $rel"
    }
}
Write-Host "Backup complete."
exit 0
