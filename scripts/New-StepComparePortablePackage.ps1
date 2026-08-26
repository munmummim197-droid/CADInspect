[CmdletBinding()]
param(
    [switch] $FreshBuild
)

$ErrorActionPreference = 'Stop'

$projectRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$staging = [IO.Path]::GetFullPath((Join-Path $projectRoot 'dist-staging'))
$destination = [IO.Path]::GetFullPath((Join-Path $projectRoot 'dist'))
foreach ($candidate in @($staging, $destination)) {
    if (-not $candidate.StartsWith($projectRoot + [IO.Path]::DirectorySeparatorChar,
                                   [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe package path: ${candidate}"
    }
}

& (Join-Path $PSScriptRoot 'Invoke-StepCompareBuild.ps1') `
    -Preset full-dev `
    -Stage All `
    -Configuration Release `
    -Fresh:$FreshBuild

if (Test-Path -LiteralPath $staging) {
    Remove-Item -LiteralPath $staging -Recurse -Force
}
New-Item -ItemType Directory -Path $staging | Out-Null

$guiOutput = Join-Path $projectRoot 'build\vs-full-dev\apps\gui\Release'
$cliOutput = Join-Path $projectRoot 'build\vs-full-dev\apps\cli\Release'
$guiExecutable = Join-Path $guiOutput 'StepCompare.exe'
$cliExecutable = Join-Path $cliOutput 'stepcompare-cli.exe'
foreach ($executable in @($guiExecutable, $cliExecutable)) {
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Required Release executable is missing: ${executable}"
    }
}

Get-ChildItem -LiteralPath $guiOutput -File |
    Where-Object Extension -In @('.exe', '.dll') |
    Copy-Item -Destination $staging
Copy-Item -LiteralPath $cliExecutable -Destination $staging
Get-ChildItem -LiteralPath $cliOutput -Filter '*.dll' -File |
    Copy-Item -Destination $staging -Force

$windeployqt = Join-Path $projectRoot `
    'vcpkg_installed\x64-windows\tools\Qt6\bin\windeployqt.exe'
if (-not (Test-Path -LiteralPath $windeployqt -PathType Leaf)) {
    throw "Pinned windeployqt is missing: ${windeployqt}"
}
& $windeployqt `
    --release `
    --no-translations `
    --no-system-d3d-compiler `
    --no-opengl-sw `
    --dir $staging `
    (Join-Path $staging 'StepCompare.exe')
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code ${LASTEXITCODE}."
}

$required = @(
    (Join-Path $staging 'StepCompare.exe'),
    (Join-Path $staging 'stepcompare-cli.exe'),
    (Join-Path $staging 'Qt6Core.dll'),
    (Join-Path $staging 'platforms\qwindows.dll')
)
foreach ($path in $required) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Portable package validation failed; missing ${path}"
    }
}

if (Test-Path -LiteralPath $destination) {
    Remove-Item -LiteralPath $destination -Recurse -Force
}
Move-Item -LiteralPath $staging -Destination $destination

$manifest = Get-ChildItem -LiteralPath $destination -Recurse -File |
    Sort-Object FullName |
    ForEach-Object {
        [PSCustomObject]@{
            Path = $_.FullName.Substring($destination.Length + 1)
            SizeBytes = $_.Length
            SHA256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }

[PSCustomObject]@{
    PackageRoot = $destination
    FileCount = @($manifest).Count
    TotalBytes = ($manifest | Measure-Object -Property SizeBytes -Sum).Sum
    StepCompareSha256 = ($manifest | Where-Object Path -EQ 'StepCompare.exe').SHA256
    CliSha256 = ($manifest | Where-Object Path -EQ 'stepcompare-cli.exe').SHA256
}
