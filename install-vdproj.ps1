$ErrorActionPreference = 'Stop'

Write-Host "Installing Visual Studio Installer Projects..." -ForegroundColor Cyan
Write-Host "Downloading..."

$vsixPath = "$($env:TEMP)\InstallerProjects.vsix"
$vsixUrl = 'https://visualstudioclient.gallerycdn.vsassets.io/extensions/visualstudioclient/microsoftvisualstudio2022installerprojects/0.1.0/1631880472071/InstallerProjects.vsix'

$web = New-Object Net.WebClient
$web.DownloadFile($vsixUrl, $vsixPath)
Write-Host "Downloaded $vsixPath"

$vsRoot = $null
foreach ($candidate in @(
    "${env:ProgramFiles}\Microsoft Visual Studio\18\Community",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Preview"
)) {
    if (Test-Path "$candidate\Common7\IDE\VSIXInstaller.exe") {
        $vsRoot = $candidate
        break
    }
}
if (-not $vsRoot) {
    throw "Could not locate VSIXInstaller.exe (tried $($candidates -join ', '))"
}
Write-Host "Visual Studio root: $vsRoot"
Write-Host "Installing..."

Start-Process "$vsRoot\Common7\IDE\VSIXInstaller.exe" "/q /a $vsixPath" -Wait
if ($LASTEXITCODE -ne 0) {
    Write-Host "VSIXInstaller exit code: $LASTEXITCODE" -ForegroundColor Red
}
Remove-Item $vsixPath -Force -ErrorAction Ignore

$disableTool = "$vsRoot\Common7\IDE\CommonExtensions\Microsoft\VSI\DisableOutOfProcBuild\DisableOutOfProcBuild.exe"
if (Test-Path $disableTool) {
    Write-Host "Disabling out-of-proc build..."
    Push-Location (Split-Path $disableTool)
    try { & $disableTool } finally { Pop-Location }
} else {
    Write-Host "DisableOutOfProcBuild.exe not found; skipping." -ForegroundColor Yellow
}

Write-Host "Installed." -ForegroundColor Green