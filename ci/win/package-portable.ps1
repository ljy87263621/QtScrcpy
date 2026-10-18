param(
    [Parameter(Mandatory = $true)][string]$QtRoot,
    [Parameter(Mandatory = $true)][string]$FfmpegRoot,
    [Parameter(Mandatory = $true)][string]$FfmpegSourceDir,
    [Parameter(Mandatory = $true)][string]$FfmpegSourceArchive,
    [Parameter(Mandatory = $true)][string]$VCRuntimeRoot,
    [string]$Destination
)
$ErrorActionPreference = 'Stop'
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$binaries = Join-Path $repository 'output/x64/Release'
if (-not $Destination) { $Destination = Join-Path $repository 'output/portable/QtScrcpy-KeymapEditor-win64' }
$Destination = [IO.Path]::GetFullPath($Destination)
if (Test-Path -LiteralPath $Destination) { throw "Destination already exists: $Destination. Choose a new directory." }
$required = @(
    (Join-Path $binaries 'QtScrcpy.exe'),
    (Join-Path $QtRoot 'bin/windeployqt.exe'),
    (Join-Path $FfmpegRoot 'bin/avcodec-58.dll'),
    (Join-Path $VCRuntimeRoot 'vcruntime140.dll'),
    (Join-Path $FfmpegSourceDir 'config.h'),
    $FfmpegSourceArchive
)
foreach ($file in $required) { if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing input: $file" } }
$configuration = Get-Content -LiteralPath (Join-Path $FfmpegSourceDir 'config.h') -Raw
if ($configuration -match '#define CONFIG_(NONFREE|GPL|VERSION3) 1') { throw 'This packager requires an LGPL FFmpeg build without GPL/nonfree/version3 options.' }
New-Item -ItemType Directory -Path $Destination | Out-Null
Copy-Item -LiteralPath (Join-Path $binaries 'QtScrcpy.exe') -Destination $Destination
Get-ChildItem -LiteralPath $binaries -Filter '*.dll' | Where-Object Name -NotMatch '^(avcodec|avformat|avutil|swscale|swresample|avdevice|avfilter)-' | Copy-Item -Destination $Destination
foreach ($name in @('avcodec-58.dll','avformat-58.dll','avutil-56.dll','swscale-5.dll','swresample-3.dll')) {
    Copy-Item -LiteralPath (Join-Path $FfmpegRoot "bin/$name") -Destination $Destination
}
foreach ($name in @('adb.exe','AdbWinApi.dll','AdbWinUsbApi.dll','scrcpy-server','sndcpy.apk','sndcpy.bat')) {
    Copy-Item -LiteralPath (Join-Path $binaries $name) -Destination $Destination
}
foreach ($name in @('keymap','config')) { Copy-Item -LiteralPath (Join-Path $repository $name) -Destination $Destination -Recurse }
Copy-Item -LiteralPath (Join-Path $repository 'LICENSE') -Destination $Destination
Copy-Item -LiteralPath (Join-Path $repository 'docs/RuntimeKeymapEditor_zh.md') -Destination (Join-Path $Destination 'README-KeymapEditor.md')
& (Join-Path $QtRoot 'bin/windeployqt.exe') --release --no-translations --no-compiler-runtime --dir $Destination (Join-Path $Destination 'QtScrcpy.exe')
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed ($LASTEXITCODE)" }
Get-ChildItem -LiteralPath $VCRuntimeRoot -Filter '*.dll' | Copy-Item -Destination $Destination
$licenses = Join-Path $Destination 'licenses'
$sources = Join-Path $Destination 'sources'
New-Item -ItemType Directory -Path $licenses, $sources | Out-Null
Get-ChildItem -LiteralPath (Join-Path $repository 'docs/licenses') -File | Copy-Item -Destination $licenses
Copy-Item -LiteralPath (Join-Path $FfmpegSourceDir 'COPYING.LGPLv2.1') -Destination (Join-Path $licenses 'FFmpeg-LGPL-2.1.txt')
Copy-Item -LiteralPath $FfmpegSourceArchive -Destination $sources
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'build-ffmpeg-lgpl.sh') -Destination $sources
Copy-Item -LiteralPath (Join-Path $FfmpegSourceDir 'ffbuild/config.mak') -Destination (Join-Path $sources 'ffmpeg-config.mak')
Copy-Item -LiteralPath (Join-Path $repository 'docs/ThirdPartyNotices.md') -Destination $licenses
$hashes = Get-ChildItem -LiteralPath $Destination -File -Recurse | Get-FileHash -Algorithm SHA256 | ForEach-Object {
    [pscustomobject]@{ path = $_.Path.Substring($Destination.Length + 1).Replace('\','/'); sha256 = $_.Hash.ToLowerInvariant() }
}
$hashes | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $Destination 'SHA256.json') -Encoding UTF8
Compress-Archive -LiteralPath $Destination -DestinationPath ($Destination + '.zip')
Write-Output "Portable directory: $Destination"
Write-Output "Portable archive: $Destination.zip"
