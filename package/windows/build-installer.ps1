# 17Live OBS Plugin NSIS Installer Build Script
# This script builds the NSIS installer for the 17Live OBS Plugin

param(
    [Parameter(Mandatory=$false)]
    [string]$Version = "1.0.0",
    
    [Parameter(Mandatory=$false)]
    [string]$BuildDir = "..\..\build_x64\rundir\Release",
    
    [Parameter(Mandatory=$false)]
    [string]$NSISPath = "${env:ProgramFiles(x86)}\NSIS\makensis.exe",
    
    [Parameter(Mandatory=$false)]
    [string]$OutputDir = ".\output"
)

# Function to check if a file exists
function Test-FileExists {
    param([string]$Path)
    if (!(Test-Path $Path)) {
        Write-Error "File not found: $Path"
        exit 1
    }
}

# Function to check if a directory exists
function Test-DirectoryExists {
    param([string]$Path)
    if (!(Test-Path $Path -PathType Container)) {
        Write-Error "Directory not found: $Path"
        exit 1
    }
}

Write-Host "17Live OBS Plugin NSIS Installer Build Script" -ForegroundColor Green
Write-Host "=============================================" -ForegroundColor Green

# Check prerequisites
Write-Host "Checking prerequisites..." -ForegroundColor Yellow

# Check if NSIS is installed
Test-FileExists $NSISPath
Write-Host "✓ NSIS found at: $NSISPath" -ForegroundColor Green

# Check if build directory exists
Test-DirectoryExists $BuildDir
Write-Host "✓ Build directory found: $BuildDir" -ForegroundColor Green

# Check if required plugin files exist
$PluginDLL = Join-Path $BuildDir "obs-17live.dll"
$PluginPDB = Join-Path $BuildDir "obs-17live.pdb"
$PluginDataDir = Join-Path $BuildDir "obs-17live"

Test-FileExists $PluginDLL
Test-FileExists $PluginPDB
Test-DirectoryExists $PluginDataDir

Write-Host "✓ Plugin files found" -ForegroundColor Green

# Create output directory if it doesn't exist
if (!(Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
    Write-Host "✓ Created output directory: $OutputDir" -ForegroundColor Green
}

# Create temporary NSI file with version substitution
$NSITemplate = Get-Content "installer.nsi" -Raw

# Extract numeric version for NSIS (remove any suffix like -stage, -beta, etc.)
$NumericVersion = $Version -replace '-.*$', ''
# Ensure we have at least 3 parts for the version (X.X.X format)
$VersionParts = $NumericVersion.Split('.')
while ($VersionParts.Length -lt 3) {
    $VersionParts += "0"
}
$CleanVersion = $VersionParts[0..2] -join '.'

# Replace PRODUCT_VERSION for NSIS version info (must be X.X.X format)
$NSIContent = $NSITemplate -replace '!define PRODUCT_VERSION "1\.0\.0"', "!define PRODUCT_VERSION `"$CleanVersion`""
# Replace the OutFile to use the full version (including suffix) for the installer filename
$NSIContent = $NSIContent -replace 'OutFile "17liveOBSPlugin-windows-v\$\{PRODUCT_VERSION\}\.exe"', "OutFile `"17liveOBSPlugin-windows-v$Version.exe`""

$TempNSI = "installer_temp.nsi"
$NSIContent | Out-File -FilePath $TempNSI -Encoding UTF8

Write-Host "✓ Created temporary NSI file with NSIS version $CleanVersion (from $Version)" -ForegroundColor Green

# Build the installer
Write-Host "Building NSIS installer..." -ForegroundColor Yellow

$NSISArgs = @(
    $TempNSI
)

try {
    $Process = Start-Process -FilePath $NSISPath -ArgumentList $NSISArgs -Wait -PassThru -NoNewWindow
    
    if ($Process.ExitCode -eq 0) {
        Write-Host "✓ NSIS installer built successfully!" -ForegroundColor Green
        
        # Move the installer to output directory
        $InstallerName = "17liveOBSPlugin-windows-v$Version.exe"
        if (Test-Path $InstallerName) {
            Move-Item $InstallerName (Join-Path $OutputDir $InstallerName) -Force
            Write-Host "✓ Installer moved to: $(Join-Path $OutputDir $InstallerName)" -ForegroundColor Green
        }
    } else {
        Write-Error "NSIS build failed with exit code: $($Process.ExitCode)"
        exit 1
    }
} catch {
    Write-Error "Failed to run NSIS: $_"
    exit 1
} finally {
    # Clean up temporary file
    if (Test-Path $TempNSI) {
        Remove-Item $TempNSI -Force
    }
}

Write-Host ""
Write-Host "Build completed successfully!" -ForegroundColor Green
Write-Host "Installer location: $(Join-Path $OutputDir "17liveOBSPlugin-windows-v$Version.exe")" -ForegroundColor Cyan

# Display file information
$InstallerPath = Join-Path $OutputDir "17liveOBSPlugin-windows-v$Version.exe"
if (Test-Path $InstallerPath) {
    $FileInfo = Get-Item $InstallerPath
    Write-Host ""
    Write-Host "Installer Information:" -ForegroundColor Yellow
    Write-Host "  File: $($FileInfo.Name)"
    Write-Host "  Size: $([math]::Round($FileInfo.Length / 1MB, 2)) MB"
    Write-Host "  Created: $($FileInfo.CreationTime)"
}