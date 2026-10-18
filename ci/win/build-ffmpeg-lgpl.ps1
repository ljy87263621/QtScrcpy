param(
    [Parameter(Mandatory = $true)][string]$SourceDir,
    [Parameter(Mandatory = $true)][string]$OutputDir,
    [string]$MsysWrapper = 'D:\Develope\Tools\CodexToolchain\codex-msys2.cmd'
)
$ErrorActionPreference = 'Stop'
if (-not $env:VCToolsInstallDir) { throw 'Run this script inside the MSVC developer environment.' }
$script = Join-Path $PSScriptRoot 'build-ffmpeg-lgpl.sh'
& $MsysWrapper sh ($script.Replace('\', '/')) $SourceDir $OutputDir
if ($LASTEXITCODE -ne 0) { throw "FFmpeg build failed ($LASTEXITCODE)." }
