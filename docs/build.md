# Build OBS 17LIVE Plugin

This guide will walk you through the process of building the OBS 17LIVE plugin from source.

## build chat room app first

```bash
cd web/ably_chat
npm install
npm run build
```

**Note: run `cmake --build --preset [macos|windows-x64]` after rebuilding the chat room app so the
plugin packaging picks up the latest web assets.**

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
cmake --build --preset windows-x64 --config RelWithDebInfo
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
cmake --build --preset macos --config RelWithDebInfo
```

Then open the generated Xcode project `build_macos/obs-17live.xcodeproj`. Build the plugin.

## CI / Release Packaging

- There are no `*-prod` presets. CI uses the same presets and injects environment-specific values
  via `-D` arguments and GitHub Actions environment variables.
- The Steam version of OBS is essentially OBS Studio. To ensure compatibility with both the official and Steam versions, the installer no longer relies on OBS’s installation directory; instead, it installs into OBS’s user plugin directory (which OBS automatically scans).
- Key injected variables:
  - `ONESEVENLIVE_API_URL` (GitHub Actions env/vars)
  - `CMAKE_PROJECT_VERSION` (derived from git tag or workflow input)
  - `YOUTUBE_API_CLIENT_ID`, `YOUTUBE_API_CLIENT_SECRET`, `TWITCH_API_CLIENT_ID` (vars)

```bash
cmake --preset macos \
  -DYOUTUBE_API_CLIENT_ID="$YOUTUBE_API_CLIENT_ID" \
  -DYOUTUBE_API_CLIENT_SECRET="$YOUTUBE_API_CLIENT_SECRET" \
  -DTWITCH_API_CLIENT_ID="$TWITCH_API_CLIENT_ID" \
  -DONESEVENLIVE_API_URL="$ONESEVENLIVE_API_URL" \
  -DCMAKE_PROJECT_VERSION="$VERSION"

cmake --build --preset macos --config Release

cmake --install build_macos --config Release --prefix "$PWD/dist-install"
# macOS pkg: dist-install/obs-17live.pkg
# CI also exports non-installer zip containing:
#   obs-17live.plugin
# copy this bundle directly into:
#   ~/Library/Application Support/obs-studio/plugins

cmake --preset windows-x64 ^
  -DYOUTUBE_API_CLIENT_ID="%YOUTUBE_API_CLIENT_ID%" ^
  -DYOUTUBE_API_CLIENT_SECRET="%YOUTUBE_API_CLIENT_SECRET%" ^
  -DTWITCH_API_CLIENT_ID="%TWITCH_API_CLIENT_ID%" ^
  -DONESEVENLIVE_API_URL="%ONESEVENLIVE_API_URL%" ^
  -DCMAKE_PROJECT_VERSION="%VERSION%"

cmake --build --preset windows-x64 --config Release
# Windows installer is built from package/windows and installs into:
# %ProgramData%\obs-studio\plugins\obs-17live
# CI also exports non-installer zip containing:
#   obs-17live/bin/64bit/obs-17live.dll
#   obs-17live/data/...
# copy the extracted obs-17live folder directly into:
#   %ProgramData%\obs-studio\plugins
```
