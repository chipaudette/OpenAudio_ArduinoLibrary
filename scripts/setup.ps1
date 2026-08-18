<#
.SYNOPSIS
    First-time setup for OpenAudio multi-channel USB audio on Teensy.
.DESCRIPTION
    1. Detects Teensyduino installation
    2. Backs up original Teensy core USB audio files
    3. Applies patched core files for multi-channel USB audio
    4. Installs library in Arduino IDE's sketchbook folder
    5. Validates the setup
#>

$ErrorActionPreference = "Stop"
$repoDir = Resolve-Path "$PSScriptRoot\.."

Write-Host "OpenAudio_ArduinoLibrary Setup"
Write-Host "================================"
Write-Host ""


# --- Detect Teensy installation ---
$possiblePaths = @(
    "$env:LOCALAPPDATA\Arduino15\packages\teensy\hardware\avr\1.62.0",
    "$env:LOCALAPPDATA\Arduino15\packages\teensy\hardware\avr\1.61.0",
    "$env:LOCALAPPDATA\Arduino15\packages\teensy\hardware\avr\1.60.0",
    "$env:LOCALAPPDATA\Arduino15\packages\teensy\hardware\avr\1.59.0",
    "$env:LOCALAPPDATA\Arduino15\packages\teensy\hardware\avr\1.58.0",
    "$env:LOCALAPPDATA\Arduino15\packages\teensy\hardware\avr\1.57.0",
    "$env:LOCALAPPDATA\Arduino15\packages\teensy\hardware\avr\1.56.0",
    "$env:LOCALAPPDATA\Arduino15\packages\teensy\hardware\avr\1.55.0",
    "$env:LOCALAPPDATA\Arduino15\packages\teensy\hardware\avr\1.54.0",
    "$env:LOCALAPPDATA\Arduino15\packages\teensy\hardware\avr\1.53.0",
    "$env:LOCALAPPDATA\Arduino15\packages\teensy\hardware\avr\1.52.0",
    "$env:LOCALAPPDATA\Arduino15\packages\teensy\hardware\avr\1.51.0",
    "$env:LOCALAPPDATA\Arduino15\packages\teensy\hardware\avr\1.50.0"
)
$teensyDir = $null
foreach ($p in $possiblePaths) {
    if (Test-Path -LiteralPath "$p\cores\teensy4\usb_desc.h") {
        $teensyDir = $p
        break
    }
}
if (-not $teensyDir) {
    Write-Error "Teensyduino installation not found. Checked:`n$($possiblePaths -join "`n")"
    exit 1
}
$version = Split-Path -Leaf $teensyDir
Write-Host "Teensyduino detected: $version"

# --- Step 1: Backup ---
$backupDir = "$PSScriptRoot\..\backups\$version"
$backupStamp = "$backupDir\cores\teensy4\usb_desc.h"
if (Test-Path -LiteralPath $backupStamp) {
    Write-Host "`nStep 1: Backup already exists at $backupDir (skipping)."
} else {
    Write-Host "`nStep 1: Backing up original Teensy core files..."
    & "$PSScriptRoot\backup_teensy_audio.ps1" -teensyDir $teensyDir -backupDir $backupDir
    if ($LASTEXITCODE -ne 0) { Write-Error "Backup failed."; exit 1 }
}

# --- Step 2: Patch core ---
Write-Host "`nStep 2: Applying patched Teensy core files..."
& "$PSScriptRoot\update_teensy_audio.ps1" -teensyDir $teensyDir
if ($LASTEXITCODE -ne 0) { Write-Error "Update failed."; exit 1 }

# --- Step 3: Install into Arduino libraries folder ---
$arduinoLibDir = "$env:USERPROFILE\Documents\Arduino\libraries"
$arduinoLibPath = "$arduinoLibDir\OpenAudio_ArduinoLibrary"
Write-Host "`nStep 3: Installing library for Arduino IDE..."
if (-not (Test-Path -LiteralPath $arduinoLibDir)) {
    New-Item -ItemType Directory -Path $arduinoLibDir | Out-Null
}
if (Test-Path -LiteralPath $arduinoLibPath) {
    Write-Host "  Updating existing library install..."
    Remove-Item -LiteralPath $arduinoLibPath -Recurse -Force
}
Copy-Item -LiteralPath $repoDir -Destination $arduinoLibPath -Recurse -Force
Write-Host "  Installed at: $arduinoLibPath"

# --- Validate ---
Write-Host "`n--- Validation ---"
$ok = $true
$checkCore = "$teensyDir\cores\teensy4\usb_audio_interface.h"
if (Test-Path -LiteralPath $checkCore) {
    Write-Host "  [OK] Patched core files present"
} else {
    Write-Host "  [FAIL] Patched core files missing at $checkCore"
    $ok = $false
}
$checkLib = "$arduinoLibPath\examples\AudioUSB2TDM\AudioUSB2TDM.ino"
if (Test-Path -LiteralPath $checkLib) {
    Write-Host "  [OK] Library examples accessible"
} else {
    Write-Host "  [WARN] Examples not found at $checkLib (library may not be linked)"
    $ok = $false
}

Write-Host ""
if ($ok) {
    Write-Host "Setup complete! Restart Arduino IDE, then open:"
    Write-Host "  File > Examples > OpenAudio_ArduinoLibrary > AudioUSB2TDM"
    Write-Host ""
    Write-Host "Backup:  .\scripts\restore_teensy_audio.ps1"
} else {
    Write-Host "Setup completed with warnings. See above."
}
