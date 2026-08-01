param(
    [string]$InstallerPath = "dist/RemoteClipboard-client-windows-x64-setup.exe"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not [System.IO.Path]::IsPathRooted($InstallerPath)) {
    $InstallerPath = Join-Path $repositoryRoot $InstallerPath
}
$InstallerPath = [System.IO.Path]::GetFullPath($InstallerPath)
if (-not (Test-Path $InstallerPath -PathType Leaf)) {
    throw "Windows installer not found: $InstallerPath"
}

$temporaryRoot = [System.IO.Path]::GetTempPath()
$testDirectoryName = "RemoteClipboardInstallerTest-" + [guid]::NewGuid().ToString("N")
$installDirectory = Join-Path $temporaryRoot $testDirectoryName
$setupArguments = @(
    "/VERYSILENT",
    "/SUPPRESSMSGBOXES",
    "/NORESTART",
    "/DIR=`"$installDirectory`""
)
$setup = Start-Process -FilePath $InstallerPath -ArgumentList $setupArguments -Wait -PassThru
if ($setup.ExitCode -ne 0) {
    throw "Installer exited with code $($setup.ExitCode)"
}

$installedApplication = Join-Path $installDirectory "bin/remote-clipboard-windows.exe"
if (-not (Test-Path $installedApplication -PathType Leaf)) {
    throw "Installer did not deploy the Windows client"
}

$uninstallerPath = Join-Path $installDirectory "unins000.exe"
if (-not (Test-Path $uninstallerPath -PathType Leaf)) {
    throw "Installer did not register an uninstaller"
}
$uninstallArguments = @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART")
$uninstall = Start-Process -FilePath $uninstallerPath -ArgumentList $uninstallArguments -Wait -PassThru
if ($uninstall.ExitCode -ne 0) {
    throw "Uninstaller exited with code $($uninstall.ExitCode)"
}
for ($attempt = 0; $attempt -lt 50 -and (Test-Path $installedApplication); $attempt++) {
    Start-Sleep -Milliseconds 100
}
if (Test-Path $installedApplication) {
    throw "Uninstaller did not remove the Windows client"
}

Write-Host "Windows installer install/uninstall verification passed"
