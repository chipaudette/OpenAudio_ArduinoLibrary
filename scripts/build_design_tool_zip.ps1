# Builds gui/DesignTool_F32.zip from the tracked working copy in docs/.
#
# The docs/ folder is the canonical (browser-runnable) Design Tool source:
# index.html, red/, bootstrap/, jquery/, orion/, font-awesome/, icons/, img/,
# style.css -- plus the *.md docs and scripts/ that are NOT shipped in the zip.
#
# This script packages everything under docs/ EXCEPT:
#   - *.md files (markdown docs)
#   - scripts/ (MATLAB node-generation helpers)
#
# Line endings: docs/ is checked out with CRLF.  The shipped zip has used LF
# historically, so text files (.html/.js/.css/.svg) are converted CRLF -> LF.
# Binary files are copied byte-for-byte.  The result is a flat zip (no root
# folder) so that after extracting it you can open index.html directly.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts\build_design_tool_zip.ps1
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

$ErrorActionPreference = "Stop"

$srcDir = Join-Path $RepoRoot "docs"
$dstZip = Join-Path $RepoRoot "gui\DesignTool_F32.zip"
$textExt = @(".html", ".js", ".css", ".svg")

if (-not (Test-Path -LiteralPath $srcDir)) {
    throw "Source folder not found: $srcDir"
}
if (-not (Test-Path -LiteralPath (Split-Path $dstZip))) {
    throw "Destination folder not found: $(Split-Path $dstZip)"
}

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

if (Test-Path -LiteralPath $dstZip) {
    Remove-Item -LiteralPath $dstZip -Force
}

$fs = [System.IO.File]::Open($dstZip, [System.IO.FileMode]::CreateNew)
$zip = New-Object System.IO.Compression.ZipArchive($fs, [System.IO.Compression.ZipArchiveMode]::Create)
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)

try {
    $files = Get-ChildItem $srcDir -Recurse -File | Where-Object {
        $_.Extension -ne ".md" -and $_.FullName -notmatch "\\scripts\\"
    }
    foreach ($f in $files) {
        $rel = $f.FullName.Substring($srcDir.Length + 1).Replace("\", "/")
        $entry = $zip.CreateEntry($rel, [System.IO.Compression.CompressionLevel]::Optimal)
        $es = $entry.Open()
        try {
            if ($textExt -contains $f.Extension.ToLower()) {
                $txt = [System.IO.File]::ReadAllText($f.FullName)
                $txt = $txt -replace "`r`n", "`n"
                $bytes = $utf8NoBom.GetBytes($txt)
                $es.Write($bytes, 0, $bytes.Length)
            } else {
                $in = [System.IO.File]::OpenRead($f.FullName)
                try { $in.CopyTo($es) } finally { $in.Close() }
            }
        } finally {
            $es.Close()
        }
    }
} finally {
    $zip.Dispose()
    $fs.Close()
}

Write-Output "Built $dstZip ($($files.Count) files) from $srcDir"
