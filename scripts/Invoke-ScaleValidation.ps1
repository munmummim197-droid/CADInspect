[CmdletBinding()]
param(
    [ValidateRange(1, 100000)]
    [int] $LargeOccurrences = 1000,

    [ValidateRange(1, 100000)]
    [int] $VeryLargeOccurrences = 5000,

    [ValidateRange(1, 10)]
    [int] $Repetitions = 3,

    [switch] $SkipGeneratorBuild
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$benchmarkSource = Join-Path $projectRoot 'benchmark'
$benchmarkBuild = Join-Path $projectRoot 'build\scale-generator'
$fixtureRoot = Join-Path $projectRoot 'build\scale-validation\fixtures'
$resultRoot = Join-Path $projectRoot 'build\scale-validation\results'
$cliPath = Join-Path $projectRoot 'dist\stepcompare-cli.exe'
$generatorPath = Join-Path $benchmarkBuild 'Release\stepcompare_fixture_generator.exe'
$vcpkgRoot = Join-Path $projectRoot '.tools\vcpkg'
$vcpkgInstalled = Join-Path $projectRoot 'vcpkg_installed'
$vcpkgBin = Join-Path $vcpkgInstalled 'x64-windows\bin'

$vsWhere = Join-Path ${env:ProgramFiles(x86)} `
    'Microsoft Visual Studio\Installer\vswhere.exe'
$installationPath = & $vsWhere -latest `
    -products Microsoft.VisualStudio.Product.BuildTools `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $installationPath) {
    throw 'Không tìm thấy Visual Studio Build Tools C++ x64.'
}
$cmake = Join-Path $installationPath `
    'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$devCmd = Join-Path $installationPath 'Common7\Tools\VsDevCmd.bat'

$developerEnvironment = @{}
$environmentLines = & $env:ComSpec /d /s /c `
    "`"${devCmd}`" -arch=x64 -host_arch=x64 >nul && set"
if ($LASTEXITCODE -ne 0) {
    throw "VsDevCmd thất bại với exit code ${LASTEXITCODE}."
}
foreach ($line in $environmentLines) {
    $separator = $line.IndexOf('=')
    if ($separator -gt 0) {
        $name = $line.Substring(0, $separator)
        $value = $line.Substring($separator + 1)
        $developerEnvironment[$name] = $value
    }
}
$developerEnvironment['VCPKG_ROOT'] = $vcpkgRoot
$developerEnvironment['PATH'] = "${vcpkgBin};$($developerEnvironment['PATH'])"

function Invoke-CleanProcess {
    param(
        [Parameter(Mandatory)] [string] $FilePath,
        [Parameter(Mandatory)] [string[]] $ArgumentList
    )
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $FilePath
    $startInfo.UseShellExecute = $false
    foreach ($argument in $ArgumentList) {
        [void] $startInfo.ArgumentList.Add($argument)
    }
    $startInfo.Environment.Clear()
    foreach ($entry in $developerEnvironment.GetEnumerator()) {
        $startInfo.Environment[[string] $entry.Key] = [string] $entry.Value
    }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    [void] $process.Start()
    $process.WaitForExit()
    $exitCode = $process.ExitCode
    $process.Dispose()
    if ($exitCode -ne 0) {
        throw "Lệnh thất bại (${exitCode}): ${FilePath}"
    }
}

function Invoke-MeasuredProcess {
    param(
        [Parameter(Mandatory)] [string] $FilePath,
        [Parameter(Mandatory)] [string[]] $ArgumentList
    )
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $FilePath
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.CreateNoWindow = $true
    foreach ($argument in $ArgumentList) {
        [void] $startInfo.ArgumentList.Add($argument)
    }
    $startInfo.Environment.Clear()
    foreach ($entry in $developerEnvironment.GetEnumerator()) {
        $startInfo.Environment[[string] $entry.Key] = [string] $entry.Value
    }

    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    $clock = [Diagnostics.Stopwatch]::StartNew()
    [void] $process.Start()
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $peakWorkingSet = 0L
    $peakPrivateBytes = 0L
    $sampleCount = 0
    while (-not $process.WaitForExit(20)) {
        $process.Refresh()
        $peakWorkingSet = [Math]::Max($peakWorkingSet, $process.WorkingSet64)
        $peakPrivateBytes = [Math]::Max($peakPrivateBytes, $process.PrivateMemorySize64)
        $sampleCount++
    }
    $process.WaitForExit()
    $clock.Stop()
    $process.Refresh()
    $peakWorkingSet = [Math]::Max($peakWorkingSet, $process.PeakWorkingSet64)
    $peakPrivateBytes = [Math]::Max($peakPrivateBytes, $process.PrivateMemorySize64)
    $cpuMilliseconds = $process.TotalProcessorTime.TotalMilliseconds
    $exitCode = $process.ExitCode
    $stdout = $stdoutTask.GetAwaiter().GetResult().Trim()
    $stderr = $stderrTask.GetAwaiter().GetResult().Trim()
    $process.Dispose()

    $aggregateCpuPercent = if ($clock.Elapsed.TotalMilliseconds -gt 0) {
        100.0 * $cpuMilliseconds / $clock.Elapsed.TotalMilliseconds
    } else { 0.0 }
    $normalizedCpuPercent = $aggregateCpuPercent / [Environment]::ProcessorCount
    return [ordered] @{
        exitCode = $exitCode
        stdout = $stdout
        stderr = $stderr
        wallMilliseconds = [Math]::Round($clock.Elapsed.TotalMilliseconds, 3)
        cpuMilliseconds = [Math]::Round($cpuMilliseconds, 3)
        aggregateCpuPercent = [Math]::Round($aggregateCpuPercent, 3)
        normalizedMachineCpuPercent = [Math]::Round($normalizedCpuPercent, 3)
        peakWorkingSetBytes = $peakWorkingSet
        peakPrivateBytes = $peakPrivateBytes
        processSampleCount = $sampleCount
    }
}

if (-not (Test-Path -LiteralPath $cliPath -PathType Leaf)) {
    throw "Không tìm thấy Release package CLI: ${cliPath}"
}
New-Item -ItemType Directory -Force -Path $fixtureRoot, $resultRoot | Out-Null

if (-not $SkipGeneratorBuild) {
    Invoke-CleanProcess -FilePath $cmake -ArgumentList @(
        '-S', $benchmarkSource,
        '-B', $benchmarkBuild,
        '-G', 'Visual Studio 18 2026',
        '-A', 'x64',
        "-DCMAKE_GENERATOR_INSTANCE=${installationPath}",
        "-DCMAKE_TOOLCHAIN_FILE=${vcpkgRoot}\scripts\buildsystems\vcpkg.cmake",
        '-DVCPKG_TARGET_TRIPLET=x64-windows',
        '-DVCPKG_MANIFEST_INSTALL=OFF',
        "-DVCPKG_INSTALLED_DIR=${vcpkgInstalled}")
    Invoke-CleanProcess -FilePath $cmake -ArgumentList @(
        '--build', $benchmarkBuild, '--config', 'Release')
}
if (-not (Test-Path -LiteralPath $generatorPath -PathType Leaf)) {
    throw "Không tìm thấy fixture generator: ${generatorPath}"
}

$cases = @(
    [ordered] @{ name = 'large'; occurrences = $LargeOccurrences },
    [ordered] @{ name = 'very-large'; occurrences = $VeryLargeOccurrences }
)
$caseEvidence = @()
foreach ($case in $cases) {
    $caseDirectory = Join-Path $fixtureRoot $case.name
    New-Item -ItemType Directory -Force -Path $caseDirectory | Out-Null
    $pathA = Join-Path $caseDirectory 'assembly-A.step'
    $pathB = Join-Path $caseDirectory 'assembly-B.step'

    $generation = Invoke-MeasuredProcess -FilePath $generatorPath `
        -ArgumentList @($pathA, $pathB, [string] $case.occurrences)
    if ($generation.exitCode -ne 0) {
        throw "Sinh fixture $($case.name) thất bại: $($generation.stderr)"
    }

    $runs = @()
    for ($iteration = 1; $iteration -le $Repetitions; $iteration++) {
        $reportPath = Join-Path $resultRoot `
            "$($case.name)-run-${iteration}.json"
        $measurement = Invoke-MeasuredProcess -FilePath $cliPath `
            -ArgumentList @($pathA, $pathB, '--deep', '--json', $reportPath)
        $measurement['iteration'] = $iteration
        $measurement['reportPath'] = $reportPath
        $measurement['reportSha256'] = if (Test-Path -LiteralPath $reportPath) {
            (Get-FileHash -LiteralPath $reportPath -Algorithm SHA256).Hash.ToLowerInvariant()
        } else { $null }
        $parsedReport = if ($null -ne $measurement.reportSha256) {
            Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
        } else { $null }
        $measurement['reportedDecision'] = if ($null -ne $parsedReport) {
            $parsedReport.verdict.decision
        } else { 'UNAVAILABLE' }
        $measurement['reportedReasons'] = if ($null -ne $parsedReport) {
            @($parsedReport.verdict.reasons)
        } else { @() }
        $measurement['reportedComponentRows'] = if ($null -ne $parsedReport) {
            @($parsedReport.components).Count
        } else { 0 }
        $measurement['passed'] =
            $measurement.exitCode -eq 0 -and
            $measurement.reportedDecision -eq 'PASS' -and
            $null -ne $measurement.reportSha256
        $measurement['completed'] =
            $measurement.exitCode -in @(0, 1, 2) -and
            $null -ne $measurement.reportSha256 -and
            $measurement.reportedComponentRows -eq $case.occurrences
        $measurement['decision'] = if ($measurement.stdout) {
            $measurement.stdout.Split(' ', 2)[0]
        } else { 'UNKNOWN' }
        $runs += [pscustomobject] $measurement
    }

    $fileA = Get-Item -LiteralPath $pathA
    $fileB = Get-Item -LiteralPath $pathB
    $caseEvidence += [pscustomobject] ([ordered] @{
        name = $case.name
        occurrencesPerFixture = $case.occurrences
        fixtureDescription =
            'STEP AP214/XCAF assembly; one asymmetric box prototype; translated occurrence grid.'
        fixtureA = [ordered] @{
            path = $fileA.FullName
            bytes = $fileA.Length
            sha256 = (Get-FileHash -LiteralPath $fileA.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
        fixtureB = [ordered] @{
            path = $fileB.FullName
            bytes = $fileB.Length
            sha256 = (Get-FileHash -LiteralPath $fileB.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
        byteIdenticalPair =
            (Get-FileHash -LiteralPath $fileA.FullName -Algorithm SHA256).Hash -eq
            (Get-FileHash -LiteralPath $fileB.FullName -Algorithm SHA256).Hash
        generation = [pscustomobject] $generation
        runs = $runs
        allRunsPassed = @($runs | Where-Object { -not $_.passed }).Count -eq 0
        allRunsCompleted =
            @($runs | Where-Object { -not $_.completed }).Count -eq 0
    })
}

$machineDiagnostics = @()
$physicalMemoryBytes = $null
$cpuName = $env:PROCESSOR_IDENTIFIER
$osCaption = [Runtime.InteropServices.RuntimeInformation]::OSDescription
$osVersion = [Environment]::OSVersion.VersionString
try {
    $computerSystem = Get-CimInstance Win32_ComputerSystem -ErrorAction Stop
    $physicalMemoryBytes = [long] $computerSystem.TotalPhysicalMemory
} catch {
    $machineDiagnostics += 'Win32_ComputerSystem unavailable; physical RAM not recorded.'
}
try {
    $operatingSystem = Get-CimInstance Win32_OperatingSystem -ErrorAction Stop
    $osCaption = $operatingSystem.Caption
    $osVersion = $operatingSystem.Version
} catch {
    $machineDiagnostics += 'Win32_OperatingSystem unavailable; RuntimeInformation fallback used.'
}
try {
    $processor = Get-CimInstance Win32_Processor -ErrorAction Stop |
        Select-Object -First 1
    $cpuName = $processor.Name.Trim()
} catch {
    $machineDiagnostics += 'Win32_Processor unavailable; PROCESSOR_IDENTIFIER fallback used.'
}
$gitHead = (& git -C $projectRoot rev-parse HEAD).Trim()
$gitWorktreeDirty = @(& git -C $projectRoot status --porcelain).Count -gt 0
$evidence = [ordered] @{
    schemaVersion = 1
    capturedUtc = [DateTime]::UtcNow.ToString('o')
    gitHead = $gitHead
    gitWorktreeDirty = $gitWorktreeDirty
    harness = [ordered] @{
        scriptPath = $PSCommandPath
        scriptSha256 = (Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash.ToLowerInvariant()
        generatorSourcePath = Join-Path $benchmarkSource 'fixture_generator.cpp'
        generatorSourceSha256 = (Get-FileHash -LiteralPath (Join-Path $benchmarkSource 'fixture_generator.cpp') -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    releaseCli = [ordered] @{
        path = $cliPath
        sha256 = (Get-FileHash -LiteralPath $cliPath -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    machine = [ordered] @{
        os = $osCaption
        osVersion = $osVersion
        cpu = $cpuName
        logicalProcessors = [Environment]::ProcessorCount
        physicalMemoryBytes = $physicalMemoryBytes
        diagnostics = $machineDiagnostics
    }
    measurement = [ordered] @{
        processSamplingIntervalMilliseconds = 20
        wallClock = 'System.Diagnostics.Stopwatch'
        cpuTime = 'Process.TotalProcessorTime'
        peakWorkingSet = 'max sampled WorkingSet64 and Process.PeakWorkingSet64'
        aggregateCpuPercent = '100 * process CPU milliseconds / wall milliseconds; may exceed 100 on multicore'
        normalizedMachineCpuPercent = 'aggregateCpuPercent / logical processor count'
        deepComparisonRequested = $true
        repetitionsPerCase = $Repetitions
    }
    limitations = [ordered] @{
        guiResponsivenessMeasured = $false
        guiResponsivenessReason = 'CLI benchmark không chứng minh message-loop responsiveness của GUI.'
        cooperativeCancellationMeasured = $false
        cooperativeCancellationReason = 'CLI không có cancellation interface; kill process không được coi là cooperative cancellation evidence.'
        coldCacheGuaranteed = $false
        coldCacheReason = 'Không xóa Windows file cache; các lần chạy được báo riêng, không gắn nhãn cold/warm.'
    }
    cases = $caseEvidence
    allCasesPassed = @($caseEvidence | Where-Object { -not $_.allRunsPassed }).Count -eq 0
    allCasesCompleted =
        @($caseEvidence | Where-Object { -not $_.allRunsCompleted }).Count -eq 0
}

$evidencePath = Join-Path $resultRoot 'scale-evidence.json'
$evidence | ConvertTo-Json -Depth 10 | Set-Content `
    -LiteralPath $evidencePath -Encoding utf8NoBOM

$invariantCulture = [Globalization.CultureInfo]::InvariantCulture
$summaryRows = foreach ($case in $caseEvidence) {
    foreach ($run in $case.runs) {
        [pscustomobject] @{
            case = $case.name
            occurrences = $case.occurrencesPerFixture
            iteration = $run.iteration
            passed = $run.passed
            completed = $run.completed
            decision = $run.decision
            reportedDecision = $run.reportedDecision
            reportedReasons = $run.reportedReasons -join '|'
            reportedComponentRows = $run.reportedComponentRows
            exitCode = $run.exitCode
            wallMilliseconds = [string]::Format(
                $invariantCulture, '{0:0.###}', $run.wallMilliseconds)
            cpuMilliseconds = [string]::Format(
                $invariantCulture, '{0:0.###}', $run.cpuMilliseconds)
            aggregateCpuPercent = [string]::Format(
                $invariantCulture, '{0:0.###}', $run.aggregateCpuPercent)
            normalizedMachineCpuPercent = [string]::Format(
                $invariantCulture, '{0:0.###}',
                $run.normalizedMachineCpuPercent)
            peakWorkingSetBytes = $run.peakWorkingSetBytes
            peakPrivateBytes = $run.peakPrivateBytes
        }
    }
}
$summaryPath = Join-Path $resultRoot 'scale-summary.csv'
$summaryRows | Export-Csv -LiteralPath $summaryPath -Encoding utf8NoBOM `
    -NoTypeInformation -UseQuotes AsNeeded

Write-Host "Evidence: ${evidencePath}"
Write-Host "Summary:  ${summaryPath}"
if (-not $evidence.allCasesCompleted) {
    throw 'Scale validation có ít nhất một process/input failure.'
}
if (-not $evidence.allCasesPassed) {
    Write-Warning 'Benchmark hoàn tất nhưng có verdict fail-closed khác PASS; xem evidence.'
}
