# Windows Installer

This directory contains the Windows installer configuration for the 17Live OBS Plugin using NSIS (Nullsoft Scriptable Install System).

## NSIS Installer Setup

The installer is built using NSIS, which provides a lightweight and flexible installation system. The configuration includes:

### Files Structure
- `installer.nsi` - Main NSIS installer script
- `nsis-config.nsh` - Configuration file with constants and macros
- `build-installer.ps1` - PowerShell build script for automation
- `build.bat` - Batch file wrapper for easy building

### Prerequisites
1. **NSIS** - Download and install from https://nsis.sourceforge.io/
2. **Built Plugin** - Ensure the plugin is built in `../../build_x64/rundir/Release/`

### Building the Installer

#### Method 1: Using Batch File (Recommended)
```cmd
# Build with default version (1.0.0)
build.bat

# Build with specific version
build.bat 2.1.0
```

#### Method 2: Using PowerShell Script
```powershell
# Build with default settings
.\build-installer.ps1

# Build with custom version
.\build-installer.ps1 -Version "2.1.0"

# Build with custom paths
.\build-installer.ps1 -Version "2.1.0" -BuildDir "..\..\build_x64\rundir\Release" -OutputDir ".\dist"
```

#### Method 3: Direct NSIS Compilation
```cmd
# Navigate to the package/windows directory
cd package\windows

# Compile with NSIS
"C:\Program Files (x86)\NSIS\makensis.exe" installer.nsi
```

### Output
The following files will be generated in the `output` directory:
- `17liveOBSPlugin-windows-v{VERSION}.exe`
- `17liveOBSPlugin-windows-v{VERSION}-non-installer.zip`

The non-installer zip already uses OBS plugin layout:
- `obs-17live/bin/64bit/obs-17live.dll`
- `obs-17live/data/...`

Users can extract and copy `obs-17live` directly into `%ProgramData%\obs-studio\plugins`.

### Installation Features
- Installs into the OBS global plugin directory (ProgramData), independent of the OBS installation path
- Works with both the official OBS build and the Steam OBS build
- Start menu shortcuts for website and uninstaller
- Registry entries for proper uninstallation
- Multi-language support (English, Japanese, Traditional Chinese)
- Upgrade handling (removes previous versions automatically)

### Registry Information
The installer maintains compatibility with the previous WiX installer by using the same:
- Package ID: `OneSevenLive.ObsPlugin`
- Upgrade Code: `b81d9705-e5b2-475f-8de8-4d02d297c073`
- Registry keys for uninstallation
