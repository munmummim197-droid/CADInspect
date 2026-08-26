[CmdletBinding()]
param(
    [ValidateSet('Configure', 'Build', 'Test', 'All')]
    [string] $Stage = 'All',

    [ValidateSet('core-dev', 'full-dev')]
    [string] $Preset = 'core-dev',

    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Debug',

    [switch] $Fresh
)

$ErrorActionPreference = 'Stop'

$vsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installationPath = & $vsWhere `
    -latest `
    -products Microsoft.VisualStudio.Product.BuildTools `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath

if (-not $installationPath) {
    throw 'Visual Studio Build Tools with the MSVC x64 workload was not found.'
}

$cmakeBin = Join-Path $installationPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
$cmake = Join-Path $cmakeBin 'cmake.exe'
$ctest = Join-Path $cmakeBin 'ctest.exe'
$devCmd = Join-Path $installationPath 'Common7\Tools\VsDevCmd.bat'
$projectRoot = Split-Path -Parent $PSScriptRoot

$developerEnvironment = @{}
$environmentLines = & $env:ComSpec /d /s /c `
    "`"${devCmd}`" -arch=x64 -host_arch=x64 >nul && set"
if ($LASTEXITCODE -ne 0) {
    throw "VsDevCmd failed with exit code ${LASTEXITCODE}."
}
foreach ($line in $environmentLines) {
    $separator = $line.IndexOf('=')
    if ($separator -gt 0) {
        $name = $line.Substring(0, $separator)
        $value = $line.Substring($separator + 1)
        $developerEnvironment[$name] = $value
    }
}
$developerEnvironment['VCPKG_ROOT'] = Join-Path $projectRoot '.tools\vcpkg'

function Invoke-CleanEnvironmentProcess {
    param(
        [Parameter(Mandatory)]
        [string] $FilePath,

        [Parameter(Mandatory)]
        [string[]] $ArgumentList
    )

    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $FilePath
    $startInfo.UseShellExecute = $false
    foreach ($argument in $ArgumentList) {
        [void]$startInfo.ArgumentList.Add($argument)
    }

    $startInfo.Environment.Clear()
    foreach ($entry in $developerEnvironment.GetEnumerator()) {
        $startInfo.Environment[[string]$entry.Key] = [string]$entry.Value
    }

    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    [void]$process.Start()
    $process.WaitForExit()
    $exitCode = $process.ExitCode
    $process.Dispose()

    if ($exitCode -ne 0) {
        throw "Command failed with exit code ${exitCode}: ${FilePath}"
    }
}

if ($Stage -in @('Configure', 'All')) {
    $arguments = @('--preset', $Preset)
    if ($Fresh) {
        $arguments += '--fresh'
    }
    Invoke-CleanEnvironmentProcess -FilePath $cmake -ArgumentList $arguments
}

if ($Stage -in @('Build', 'All')) {
    Invoke-CleanEnvironmentProcess `
        -FilePath $cmake `
        -ArgumentList @('--build', '--preset', $Preset, '--config', $Configuration)
}

if ($Stage -in @('Test', 'All')) {
    Invoke-CleanEnvironmentProcess `
        -FilePath $ctest `
        -ArgumentList @('--preset', $Preset, '-C', $Configuration)
}
