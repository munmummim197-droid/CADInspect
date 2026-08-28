[CmdletBinding()]
param(
    [switch] $FreshBuild,
    [switch] $SkipBuild,
    [ValidateSet('full-dev', 'oss-release')]
    [string] $Preset = 'full-dev',
    [string] $BuildDirectory = 'build\vs-full-dev',
    [string] $Triplet = 'x64-windows'
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

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'Invoke-StepCompareBuild.ps1') `
        -Preset $Preset `
        -Stage All `
        -Configuration Release `
        -Fresh:$FreshBuild
}

if (Test-Path -LiteralPath $staging) {
    Remove-Item -LiteralPath $staging -Recurse -Force
}
New-Item -ItemType Directory -Path $staging | Out-Null

$resolvedBuildDirectory = [IO.Path]::GetFullPath(
    (Join-Path $projectRoot $BuildDirectory))
if (-not $resolvedBuildDirectory.StartsWith(
        $projectRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "Unsafe build directory: ${resolvedBuildDirectory}"
}
$guiOutput = Join-Path $resolvedBuildDirectory 'apps\gui\Release'
$cliOutput = Join-Path $resolvedBuildDirectory 'apps\cli\Release'
$guiExecutable = Join-Path $guiOutput 'CADInspect.exe'
$cliExecutable = Join-Path $cliOutput 'stepcompare-cli.exe'
foreach ($executable in @($guiExecutable, $cliExecutable)) {
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Required Release executable is missing: ${executable}"
    }
}

Copy-Item -LiteralPath $guiExecutable -Destination $staging
Get-ChildItem -LiteralPath $guiOutput -Filter '*.dll' -File |
    Copy-Item -Destination $staging -Force
Copy-Item -LiteralPath $cliExecutable -Destination $staging
Get-ChildItem -LiteralPath $cliOutput -Filter '*.dll' -File |
    Copy-Item -Destination $staging -Force

$windeployqt = Join-Path $projectRoot `
    "vcpkg_installed\${Triplet}\tools\Qt6\bin\windeployqt.exe"
if (-not (Test-Path -LiteralPath $windeployqt -PathType Leaf)) {
    throw "Pinned windeployqt is missing: ${windeployqt}"
}
& $windeployqt `
    --release `
    --no-translations `
    --no-system-d3d-compiler `
    --no-opengl-sw `
    --dir $staging `
    (Join-Path $staging 'CADInspect.exe')
$deploymentMode = 'windeployqt'
if ($LASTEXITCODE -ne 0) {
    $deploymentMode = 'pinned-vcpkg-applocal-fallback'
    Write-Warning "windeployqt failed with exit code ${LASTEXITCODE}; validating the explicit pinned vcpkg/AppLocal fallback."
}

# The vcpkg applocal integration has already copied all linked Qt/OCCT DLLs.
# Copy the minimal runtime plugins explicitly as a deterministic fallback for
# restricted hosts where windeployqt cannot spawn qtpaths.
$qtPluginRoot = Join-Path $projectRoot `
    "vcpkg_installed\${Triplet}\Qt6\plugins"
$pluginFiles = @(
    'platforms\qwindows.dll',
    'imageformats\qgif.dll',
    'imageformats\qico.dll',
    'styles\qmodernwindowsstyle.dll'
)
foreach ($relativePlugin in $pluginFiles) {
    $sourcePlugin = Join-Path $qtPluginRoot $relativePlugin
    $targetPlugin = Join-Path $staging $relativePlugin
    if (-not (Test-Path -LiteralPath $sourcePlugin -PathType Leaf)) {
        throw "Pinned Qt plugin is missing: ${sourcePlugin}"
    }
    New-Item -ItemType Directory -Path (Split-Path -Parent $targetPlugin) -Force |
        Out-Null
    Copy-Item -LiteralPath $sourcePlugin -Destination $targetPlugin -Force
}

$vsWhere = Join-Path ${env:ProgramFiles(x86)} `
    'Microsoft Visual Studio\Installer\vswhere.exe'
$installationPath = & $vsWhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$redistRoot = Join-Path $installationPath 'VC\Redist\MSVC'
$redistVersion = Get-ChildItem -LiteralPath $redistRoot -Directory |
    Where-Object Name -Match '^\d' |
    Sort-Object Name -Descending |
    Select-Object -First 1
if (-not $redistVersion) {
    throw 'MSVC x64 redistributable directory was not found.'
}
$crtDirectory = Get-ChildItem -LiteralPath (Join-Path $redistVersion.FullName 'x64') `
    -Directory -Filter 'Microsoft.VC*.CRT' |
    Sort-Object Name -Descending |
    Select-Object -First 1 -ExpandProperty FullName
if (-not $crtDirectory) {
    throw 'MSVC x64 CRT redistributable directory was not found.'
}
Get-ChildItem -LiteralPath $crtDirectory -Filter '*.dll' -File |
    Copy-Item -Destination $staging -Force

# Preserve the application license and the exact vcpkg license metadata used by
# this build. Binary packages are incomplete without these notices.
$projectLicense = Join-Path $projectRoot 'LICENSE'
$thirdPartyNotices = Join-Path $projectRoot 'THIRD_PARTY_NOTICES.md'
foreach ($notice in @($projectLicense, $thirdPartyNotices)) {
    if (-not (Test-Path -LiteralPath $notice -PathType Leaf)) {
        throw "Required release notice is missing: ${notice}"
    }
    Copy-Item -LiteralPath $notice -Destination $staging -Force
}

$licenseDirectory = Join-Path $staging 'licenses'
New-Item -ItemType Directory -Path $licenseDirectory -Force | Out-Null
$vcpkgShare = Join-Path $projectRoot "vcpkg_installed\${Triplet}\share"
$dependencyNotices = @(Get-ChildItem -LiteralPath $vcpkgShare -Filter copyright `
    -File -Recurse)
if ($dependencyNotices.Count -eq 0) {
    throw 'No vcpkg dependency license metadata was found.'
}
foreach ($notice in $dependencyNotices) {
    $packageName = $notice.Directory.Name
    Copy-Item -LiteralPath $notice.FullName `
        -Destination (Join-Path $licenseDirectory "${packageName}.txt") -Force
}

$required = @(
    (Join-Path $staging 'CADInspect.exe'),
    (Join-Path $staging 'stepcompare-cli.exe'),
    (Join-Path $staging 'Qt6Core.dll'),
    (Join-Path $staging 'Qt6Gui.dll'),
    (Join-Path $staging 'Qt6Widgets.dll'),
    (Join-Path $staging 'TKernel.dll'),
    (Join-Path $staging 'TKOpenGl.dll'),
    (Join-Path $staging 'platforms\qwindows.dll'),
    (Join-Path $staging 'imageformats\qico.dll'),
    (Join-Path $staging 'styles\qmodernwindowsstyle.dll'),
    (Join-Path $staging 'LICENSE'),
    (Join-Path $staging 'THIRD_PARTY_NOTICES.md')
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

$manifestPath = Join-Path $destination 'manifest-sha256.json'
$manifestDocument = [PSCustomObject]@{
    SchemaVersion = 1
    Product = 'CADInspect'
    Version = '0.1.0'
    DeploymentMode = $deploymentMode
    Files = @($manifest)
}
$manifestJson = $manifestDocument | ConvertTo-Json -Depth 5
[IO.File]::WriteAllText($manifestPath, $manifestJson,
    [Text.UTF8Encoding]::new($false))

[PSCustomObject]@{
    PackageRoot = $destination
    FileCount = @($manifest).Count + 1
    TotalBytes = (($manifest | Measure-Object -Property SizeBytes -Sum).Sum +
        (Get-Item -LiteralPath $manifestPath).Length)
    CADInspectSha256 = ($manifest | Where-Object Path -EQ 'CADInspect.exe').SHA256
    CliSha256 = ($manifest | Where-Object Path -EQ 'stepcompare-cli.exe').SHA256
    ManifestSha256 = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
}
