# Enhanced version updater script
# Usage: .\update_version.ps1 [action] [values...]
# Examples:
#   .\update_version.ps1 build        # Increment build number
#   .\update_version.ps1 patch        # Increment patch version
#   .\update_version.ps1 minor        # Increment minor version  
#   .\update_version.ps1 major        # Increment major version
#   .\update_version.ps1 set 1 2 3    # Set to version 1.2.3
#   .\update_version.ps1 show         # Show current version

param(
    [Parameter(Position=0)]
    [string]$Action = "show",
    
    [Parameter(Position=1)]
    [int]$Major = -1,
    
    [Parameter(Position=2)]
    [int]$Minor = -1,
    
    [Parameter(Position=3)]
    [int]$Patch = -1
)

$versionFile = "Version.h"

if (!(Test-Path $versionFile)) {
    Write-Host "Version.h not found at: $versionFile" -ForegroundColor Red
    exit 1
}

# Read current version
$content = Get-Content $versionFile -Raw

# Extract current values
$majorMatch = [regex]::Match($content, '#define VERSION_MAJOR (\d+)')
$minorMatch = [regex]::Match($content, '#define VERSION_MINOR (\d+)')
$patchMatch = [regex]::Match($content, '#define VERSION_PATCH (\d+)')
$buildMatch = [regex]::Match($content, '#define VERSION_BUILD (\d+)')

if (!$majorMatch.Success -or !$minorMatch.Success -or !$patchMatch.Success -or !$buildMatch.Success) {
    Write-Host "Could not parse version numbers from Version.h" -ForegroundColor Red
    exit 1
}

$currentMajor = [int]$majorMatch.Groups[1].Value
$currentMinor = [int]$minorMatch.Groups[1].Value
$currentPatch = [int]$patchMatch.Groups[1].Value
$currentBuild = [int]$buildMatch.Groups[1].Value

# For "show" action, just display and exit
if ($Action.ToLower() -eq "show") {
    Write-Host "Current version: $currentMajor.$currentMinor.$currentPatch.$currentBuild" -ForegroundColor Cyan
    exit 0
}

Write-Host "Current version: $currentMajor.$currentMinor.$currentPatch.$currentBuild" -ForegroundColor Cyan

# Calculate new version based on action
switch ($Action.ToLower()) {
    "build" {
        $newMajor = $currentMajor
        $newMinor = $currentMinor
        $newPatch = $currentPatch
        $newBuild = $currentBuild + 1
    }
    "patch" {
        $newMajor = $currentMajor
        $newMinor = $currentMinor
        $newPatch = $currentPatch + 1
        $newBuild = 0
    }
    "minor" {
        $newMajor = $currentMajor
        $newMinor = $currentMinor + 1
        $newPatch = 0
        $newBuild = 0
    }
    "major" {
        $newMajor = $currentMajor + 1
        $newMinor = 0
        $newPatch = 0
        $newBuild = 0
    }
    "set" {
        if ($Major -ge 0 -and $Minor -ge 0 -and $Patch -ge 0) {
            $newMajor = $Major
            $newMinor = $Minor
            $newPatch = $Patch
            $newBuild = 0
        } else {
            Write-Host "Usage: .\update_version.ps1 set [major] [minor] [patch]" -ForegroundColor Red
            exit 1
        }
    }
    default {
        Write-Host "Unknown action: $Action" -ForegroundColor Red
        Write-Host "Valid actions: build, patch, minor, major, set, show" -ForegroundColor Yellow
        exit 1
    }
}

# Update the content
$newContent = $content
$newContent = $newContent -replace '#define VERSION_MAJOR \d+', "#define VERSION_MAJOR $newMajor"
$newContent = $newContent -replace '#define VERSION_MINOR \d+', "#define VERSION_MINOR $newMinor"  
$newContent = $newContent -replace '#define VERSION_PATCH \d+', "#define VERSION_PATCH $newPatch"
$newContent = $newContent -replace '#define VERSION_BUILD \d+', "#define VERSION_BUILD $newBuild"

# Write back to file
Set-Content -Path $versionFile -Value $newContent -NoNewline

Write-Host "Updated to version: $newMajor.$newMinor.$newPatch.$newBuild" -ForegroundColor Green

# Show what changed
if ($Action -ne "set") {
    Write-Host "Changed: $Action incremented" -ForegroundColor Yellow
}