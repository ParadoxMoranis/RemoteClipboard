param(
    [string]$PackageRoot = "dist/RemoteClipboard-client-windows-x64",
    [string]$OutputDirectory = "dist"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Resolve-RepositoryPath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot $Path))
}

$cmakeContents = Get-Content (Join-Path $repositoryRoot "CMakeLists.txt") -Raw
$versionMatch = [regex]::Match(
    $cmakeContents,
    'project\(RemoteClipboard VERSION ([0-9]+\.[0-9]+\.[0-9]+) LANGUAGES CXX\)'
)
if (-not $versionMatch.Success) {
    throw "Unable to read the project version from CMakeLists.txt"
}
$version = $versionMatch.Groups[1].Value

$packageRootPath = Resolve-RepositoryPath $PackageRoot
$applicationPath = Join-Path $packageRootPath "bin/remote-clipboard-windows.exe"
if (-not (Test-Path $applicationPath -PathType Leaf)) {
    throw "Staged Windows client not found: $applicationPath"
}

$outputDirectoryPath = Resolve-RepositoryPath $OutputDirectory
New-Item -ItemType Directory -Path $outputDirectoryPath -Force | Out-Null

$isccCandidates = @(
    (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6/ISCC.exe"),
    (Join-Path $env:ProgramFiles "Inno Setup 6/ISCC.exe")
)
$isccPath = $isccCandidates | Where-Object { Test-Path $_ -PathType Leaf } | Select-Object -First 1
if (-not $isccPath) {
    $isccCommand = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($isccCommand) {
        $isccPath = $isccCommand.Source
    }
}
if (-not $isccPath) {
    throw "Inno Setup 6 compiler (ISCC.exe) was not found"
}

$installerScript = Join-Path $repositoryRoot "packaging/windows/RemoteClipboard.iss"
$isccArguments = @(
    "/DMyAppVersion=$version",
    "/DSourceRoot=$packageRootPath",
    "/DOutputRoot=$outputDirectoryPath",
    $installerScript
)
& $isccPath $isccArguments
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup failed with exit code $LASTEXITCODE"
}

$installerPath = Join-Path $outputDirectoryPath "RemoteClipboard-client-windows-x64-setup.exe"
if (-not (Test-Path $installerPath -PathType Leaf)) {
    throw "Installer was not generated: $installerPath"
}

Write-Host "Windows installer staged at $installerPath"
