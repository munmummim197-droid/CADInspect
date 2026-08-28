[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $PackageRoot,
    [Parameter(Mandatory)]
    [string] $InstallerPath,
    [Parameter(Mandatory)]
    [string] $Destination,
    [Parameter(Mandatory)]
    [ValidatePattern('^[0-9a-fA-F]{40}$')]
    [string] $CommitSha,
    [Parameter(Mandatory)]
    [string] $WorkflowRunId
)

$ErrorActionPreference = 'Stop'
$canonicalVersion = '0.1.0'
$package = [IO.Path]::GetFullPath($PackageRoot)
$installer = [IO.Path]::GetFullPath($InstallerPath)
$artifact = [IO.Path]::GetFullPath($Destination)

if (-not (Test-Path -LiteralPath $package -PathType Container)) {
    throw "Portable package is missing: ${package}"
}
if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) {
    throw "Installer is missing: ${installer}"
}
if (Test-Path -LiteralPath $artifact) {
    throw "Trusted artifact destination must not already exist: ${artifact}"
}

$gui = Join-Path $package 'CADInspect.exe'
$cli = Join-Path $package 'stepcompare-cli.exe'
$packageManifest = Join-Path $package 'manifest-sha256.json'
foreach ($required in @($gui, $cli, $packageManifest)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required trusted-build input is missing: ${required}"
    }
}

$forbidden = @(Get-ChildItem -LiteralPath $package -Recurse -File |
    Where-Object Extension -Match '^\.(pdb|obj|lib|exp|step|stp|iges|igs|brep|c|cc|cpp|cxx|h|hpp|cmake|ps1|iss|yml|yaml)$')
if ($forbidden.Count -ne 0) {
    throw "Forbidden development/test file in portable package: $($forbidden[0].FullName)"
}

$ownedDirectory = Join-Path $artifact 'unsigned-owned-pe'
$portableDirectory = Join-Path $artifact 'portable'
$installerDirectory = Join-Path $artifact 'installer'
New-Item -ItemType Directory -Path $ownedDirectory, $portableDirectory, `
    $installerDirectory | Out-Null
Copy-Item -LiteralPath $gui, $cli -Destination $ownedDirectory
Copy-Item -Path (Join-Path $package '*') -Destination $portableDirectory `
    -Recurse
Copy-Item -LiteralPath $installer -Destination $installerDirectory

$relativeFiles = @(Get-ChildItem -LiteralPath $artifact -Recurse -File |
    Sort-Object FullName |
    ForEach-Object {
        [PSCustomObject]@{
            Path = $_.FullName.Substring($artifact.Length + 1).Replace('\', '/')
            SizeBytes = $_.Length
            SHA256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    })

$manifest = [PSCustomObject]@{
    SchemaVersion = 1
    Product = 'CADInspect'
    Version = $canonicalVersion
    SourceCommit = $CommitSha.ToLowerInvariant()
    WorkflowRunId = $WorkflowRunId
    SigningStatus = 'UNSIGNED'
    SigningBoundary = [PSCustomObject]@{
        ProjectOwned = @('CADInspect.exe', 'stepcompare-cli.exe',
                         'CADInspect-Setup-x64-0.1.0.exe')
        UpstreamRuntime = 'DO_NOT_SIGN_AS_CADINSPECT'
    }
    Files = $relativeFiles
}
$manifestPath = Join-Path $artifact 'trusted-build-manifest.json'
[IO.File]::WriteAllText(
    $manifestPath,
    ($manifest | ConvertTo-Json -Depth 6),
    [Text.UTF8Encoding]::new($false))

[PSCustomObject]@{
    ArtifactRoot = $artifact
    Version = $canonicalVersion
    SourceCommit = $CommitSha.ToLowerInvariant()
    PayloadFileCount = $relativeFiles.Count
    ManifestSha256 = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
}
