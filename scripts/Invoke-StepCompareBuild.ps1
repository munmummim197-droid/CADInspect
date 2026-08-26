[CmdletBinding()]
param(
    [ValidateSet('Configure', 'Build', 'Test', 'All')]
    [string] $Stage = 'All',

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

function Invoke-CleanEnvironmentProcess {
    param(
        [Parameter(Mandatory)]
        [string] $FilePath,

        [Parameter(Mandatory)]
        [string[]] $ArgumentList
    )

    $environment = [Environment]::GetEnvironmentVariables()
    $pathValues = @()
    foreach ($key in @($environment.Keys)) {
        if ([string]$key -ieq 'Path') {
            $pathValues += [string]$environment[$key]
        }
    }
    $normalizedPath = $pathValues -join ';'

    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $FilePath
    $startInfo.UseShellExecute = $false
    foreach ($argument in $ArgumentList) {
        [void]$startInfo.ArgumentList.Add($argument)
    }

    foreach ($key in @($startInfo.Environment.Keys)) {
        if ([string]$key -ieq 'Path') {
            [void]$startInfo.Environment.Remove([string]$key)
        }
    }
    $startInfo.Environment['Path'] = $normalizedPath

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
    $arguments = @('--preset', 'core-dev')
    if ($Fresh) {
        $arguments += '--fresh'
    }
    Invoke-CleanEnvironmentProcess -FilePath $cmake -ArgumentList $arguments
}

if ($Stage -in @('Build', 'All')) {
    Invoke-CleanEnvironmentProcess `
        -FilePath $cmake `
        -ArgumentList @('--build', '--preset', 'core-dev')
}

if ($Stage -in @('Test', 'All')) {
    Invoke-CleanEnvironmentProcess `
        -FilePath $ctest `
        -ArgumentList @('--preset', 'core-dev')
}

