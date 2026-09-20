param(
    [string]$Version,
    [string]$BuildNumber = $env:APPVEYOR_BUILD_NUMBER
)

if (-not $BuildNumber) {
    $BuildNumber = "1"
}

if (-not $Version) {
    if ($env:RELEASE_TAG) {
        $Version = $env:RELEASE_TAG
    } elseif ($env:APPVEYOR_REPO_TAG_NAME) {
        $Version = $env:APPVEYOR_REPO_TAG_NAME
    }
}

# If Version is still not provided, generate tag dynamically based on date
if (-not $Version) {
    $now = Get-Date
    # Format: yyyy.M.d (e.g., 2026.9.20)
    $dateTag = $now.ToString("yyyy.M.d")

    # Check if this base date tag already exists on remote origin or locally
    $tagExists = $false
    try {
        $remoteTags = git ls-remote --tags origin 2>$null
        if ($remoteTags -and ($remoteTags -match "refs/tags/$([regex]::Escape($dateTag))($|\^|\s)")) {
            $tagExists = $true
        }
    } catch {
        $tagExists = $false
    }

    if (-not $tagExists) {
        try {
            $localTags = git tag -l $dateTag 2>$null
            if ($localTags -contains $dateTag) {
                $tagExists = $true
            }
        } catch {
            $tagExists = $false
        }
    }

    if ($tagExists) {
        $Version = "$dateTag-$BuildNumber"
    } else {
        $Version = "$dateTag"
    }
}

# Export environment variable for CI deployment step and child processes
$env:RELEASE_TAG = $Version
[System.Environment]::SetEnvironmentVariable('RELEASE_TAG', $Version, [System.EnvironmentVariableTarget]::Process)
[System.Environment]::SetEnvironmentVariable('RELEASE_TAG', $Version, [System.EnvironmentVariableTarget]::User)
if (Get-Command Set-AppveyorBuildVariable -ErrorAction SilentlyContinue) {
    Set-AppveyorBuildVariable -Name "RELEASE_TAG" -Value $Version
}

$cleanVer = $Version.TrimStart('v', 'V')
$parts = $cleanVer.Split('.-_')

$rawMajor = [int]$parts[0]
$rcMajor = $rawMajor
if ($rawMajor -gt 255) {
    $vdMajor = $rawMajor % 100
} else {
    $vdMajor = $rawMajor
}

$rcMinor = if ($parts.Length -gt 1) { [int]$parts[1] } else { 0 }
$vdMinor = $rcMinor

$rcPatch = if ($parts.Length -gt 2) { [int]$parts[2] } else { 0 }

# For MSI ProductVersion, build number ensures monotonically increasing version for major upgrades
$vdPatch = [int]$BuildNumber

$rcBuild = if ($parts.Length -gt 3) { [int]$parts[3] } else { [int]$BuildNumber }
$rcDisplayVersion = $cleanVer

$msiVersion = "$vdMajor.$vdMinor.$vdPatch"
$rcTuple = "$rcMajor,$rcMinor,$rcPatch,$rcBuild"

Write-Host "Updating version info:"
Write-Host "  Release Tag:        $Version"
Write-Host "  MSI ProductVersion: $msiVersion"
Write-Host "  RC Tuple:           $rcTuple"
Write-Host "  RC Display Version: $rcDisplayVersion"

# Update WinKillSetup.vdproj
$vdprojPath = Join-Path $PSScriptRoot "WinKillSetup\WinKillSetup.vdproj"
if (Test-Path $vdprojPath) {
    $vdproj = [System.IO.File]::ReadAllText($vdprojPath)
    $newProductCode = "{" + [Guid]::NewGuid().ToString().ToUpper() + "}"
    $newPackageCode = "{" + [Guid]::NewGuid().ToString().ToUpper() + "}"

    $vdproj = [System.Text.RegularExpressions.Regex]::Replace($vdproj, '("ProductCode"\s*=\s*"8:)\{[^}]+\}(")', "`${1}$newProductCode`$2")
    $vdproj = [System.Text.RegularExpressions.Regex]::Replace($vdproj, '("PackageCode"\s*=\s*"8:)\{[^}]+\}(")', "`${1}$newPackageCode`$2")
    $vdproj = [System.Text.RegularExpressions.Regex]::Replace($vdproj, '("ProductVersion"\s*=\s*"8:)[^"]+(")', "`${1}$msiVersion`$2")

    [System.IO.File]::WriteAllText($vdprojPath, $vdproj, [System.Text.Encoding]::UTF8)
    Write-Host "  Updated $vdprojPath"
    Write-Host "    New ProductCode: $newProductCode"
    Write-Host "    New PackageCode: $newPackageCode"
}

# Update WinKill.rc
$rcPath = Join-Path $PSScriptRoot "WinKill\WinKill.rc"
if (Test-Path $rcPath) {
    $rc = [System.IO.File]::ReadAllText($rcPath)

    $rc = [System.Text.RegularExpressions.Regex]::Replace($rc, '(?m)^(\s*FILEVERSION\s+)\d+,\d+,\d+,\d+', "`${1}$rcTuple")
    $rc = [System.Text.RegularExpressions.Regex]::Replace($rc, '(?m)^(\s*PRODUCTVERSION\s+)\d+,\d+,\d+,\d+', "`${1}$rcTuple")
    $rc = [System.Text.RegularExpressions.Regex]::Replace($rc, '(?m)^(\s*VALUE\s+"FileVersion",\s*)"[^"]+"', "`${1}""$rcDisplayVersion""" )
    $rc = [System.Text.RegularExpressions.Regex]::Replace($rc, '(?m)^(\s*VALUE\s+"ProductVersion",\s*)"[^"]+"', "`${1}""$rcDisplayVersion""" )

    [System.IO.File]::WriteAllText($rcPath, $rc, [System.Text.Encoding]::UTF8)
    Write-Host "  Updated $rcPath"
}
