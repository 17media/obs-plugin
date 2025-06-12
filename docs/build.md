# Build OBS 17LIVE Plugin

This guide will walk you through the process of building the OBS 17LIVE plugin from source.

## build chat room app first

```bash
cd web/ably_chat
npm install
npm run build
```

**Note: run `cmake --preset [macos|windows-x64]` every time you make changes to the chat room app.**

## Windows x64

Firstly install prerequisites based on [Build Instructions For Windows](https://github.com/obsproject/obs-studio/wiki/build-instructions-for-windows).

* Windows 10 1909+ (or Windows 11)
* Visual Studio 2022 (at least Community Edition)
  * Version 17.13.2 (or greater)
  * Windows 11 SDK (minimum 10.0.22621.0)
  * C++ ATL for latest v143 build tools (x86 & x64)
  * MSVC v143 - VS 2022 C++ x64/x86 build tools (Latest)
* Git for Windows
* CMake 3.28 or newer

Then build the plugin:

```bash
cmake --preset windows-x64
```

Then open the generated solution file `build_x64\obs-17live.sln` in Visual Studio. Build the plugin. 

**Note: Release build is mandatory, because obs-studio does not support debug builds.**

## macOS

Firstly install prerequisites based on [Build Instructions For Mac](https://github.com/obsproject/obs-studio/wiki/Build-Instructions-For-Mac).

* macOS 14.1 (minimum: macOS 13.5)
* Xcode 15.4
* CMake 3.30 (minimum: CMake 3.28)
* CCache 4.8 or newer (Optional)

Then build the plugin, architecture will be automatically detected:

```bash
cmake --preset macos
```

Then open the generated Xcode project `build_macos/obs-17live.xcodeproj`. Build the plugin.

## production build

To enable API url for production

```bash
cmake --preset macos-prod
cmake --preset windows-x64-prod
```