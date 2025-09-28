# PowerShell script to update version with GitHub Actions build number
# Usage: .\update-version-github.ps1 [version] [build_number]

param(
    [string]$Version = "",
    [string]$BuildNumber = ""
)

$VersionFile = "Soldato\Version.h"

# Get version from git tag if not provided
if ($Version -eq "") {
    $gitTag = git describe --tags --abbrev=0 2>$null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "No git tags found. This script requires at least one git tag." -ForegroundColor Red
        exit 1
    } else {
        # Remove 'v' prefix if present
        $Version = $gitTag -replace '^v', ''
        Write-Host "Found git tag: $gitTag -> Version: $Version" -ForegroundColor Green
    }
}

# Get build number from GitHub Actions if not provided
if ($BuildNumber -eq "") {
    $BuildNumber = $env:GITHUB_RUN_NUMBER
    if ($null -eq $BuildNumber -or $BuildNumber -eq "") {
        Write-Host "No GitHub Actions build number found. This script must be run in a GitHub Actions environment." -ForegroundColor Red
        exit 1
    } else {
        Write-Host "Found GitHub Actions build number: $BuildNumber" -ForegroundColor Green
    }
}

# Parse version components
# Remove prerelease identifiers (e.g., -test, -alpha, -beta) from patch version
$baseVersion = $Version -replace '-.*$', ''
$versionParts = $baseVersion -split '\.'
if ($versionParts.Length -lt 3) {
    Write-Host "Invalid version format. Expected format: major.minor.patch (e.g., 1.0.0)" -ForegroundColor Red
    exit 1
}

$Major = $versionParts[0]
$Minor = $versionParts[1]
$Patch = $versionParts[2]

Write-Host "Updating version to: $Major.$Minor.$Patch.$BuildNumber" -ForegroundColor Cyan

# Read current version file
$content = Get-Content $VersionFile -Raw

# Update version definitions
$content = $content -replace '#define SOLDATO_VERSION_MAJOR \d+', "#define SOLDATO_VERSION_MAJOR $Major"
$content = $content -replace '#define SOLDATO_VERSION_MINOR \d+', "#define SOLDATO_VERSION_MINOR $Minor"
$content = $content -replace '#define SOLDATO_VERSION_PATCH \d+', "#define SOLDATO_VERSION_PATCH $Patch"
$content = $content -replace '#define SOLDATO_VERSION_BUILD \d+', "#define SOLDATO_VERSION_BUILD $BuildNumber"

# Write updated content back to file
Set-Content $VersionFile -Value $content -NoNewline

Write-Host "Version updated successfully in $VersionFile" -ForegroundColor Green
Write-Host "Current version: $Major.$Minor.$Patch.$BuildNumber" -ForegroundColor Cyan
Write-Host "Display version: $Major.$Minor.$Patch" -ForegroundColor Cyan
Write-Host "About dialog will show: Soldato Version $Major.$Minor.$Patch" -ForegroundColor Cyan
Write-Host "Build info: Build $BuildNumber" -ForegroundColor Cyan

# Output for GitHub Actions
Write-Host "::set-output name=version::$Major.$Minor.$Patch.$BuildNumber"
Write-Host "::set-output name=display_version::$Major.$Minor.$Patch"
Write-Host "::set-output name=build_number::$BuildNumber"
