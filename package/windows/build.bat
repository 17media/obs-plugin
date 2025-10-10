@echo off
REM 17Live OBS Plugin NSIS Installer Build Script (Batch wrapper)
REM This batch file provides a simple entry point for building the installer

setlocal enabledelayedexpansion

echo 17Live OBS Plugin NSIS Installer Build
echo =====================================

REM Check if PowerShell is available
powershell -Command "Get-Host" >nul 2>&1
if errorlevel 1 (
    echo Error: PowerShell is required but not found.
    echo Please install PowerShell or run the build-installer.ps1 script directly.
    exit /b 1
)

REM Get version from command line argument or use default
set "VERSION=1.0.0"
if not "%1"=="" set "VERSION=%1"

echo Building installer with version: %VERSION%
echo.

REM Run the PowerShell build script
powershell -ExecutionPolicy Bypass -File "build-installer.ps1" -Version "%VERSION%"

if errorlevel 1 (
    echo.
    echo Build failed!
    exit /b 1
)

echo.
echo Build completed successfully!