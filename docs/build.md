# Build OBS 17LIVE Plugin

This guide will walk you through the process of building the OBS 17LIVE plugin from source.

## build chat room app first

`ENABLE_YOUTUBE` defaults to `false`. Only when it is explicitly set to `true` will the web chat
show the YouTube channel and the plugin expose YouTube-related multi-RTMP functionality.

```bash
cd web/ably_chat
npm install
npm run build

# Enable YouTube explicitly when needed
ENABLE_YOUTUBE=true npm run build
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

# Enable YouTube explicitly when needed
cmake --preset windows-x64 -DENABLE_YOUTUBE=ON
cmake --build --preset windows-x64 --config RelWithDebInfo
```

Then open the generated solution file `build_x64\obs-17live.sln` in Visual Studio. Build the plugin. 

**Note: Debug builds are not supported by obs-studio. Use `Release` or `RelWithDebInfo`.**

### Packaging (manual)

To build the Windows installer (`.exe`) manually, you need NSIS installed (so `makensis.exe` is available).

Build the plugin with `Release` (the installer script defaults to `build_x64\rundir\Release`):

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64 --config Release

# Enable YouTube explicitly when needed
cmake --preset windows-x64 -DENABLE_YOUTUBE=ON
cmake --build --preset windows-x64 --config Release

pwsh -ExecutionPolicy Bypass -File package/windows/build-installer.ps1 -Version "v1.2.3"
```

Outputs:

- `package/windows/output/17liveOBSPlugin-windows-v1.2.3.exe`
- `package/windows/output/17liveOBSPlugin-windows-v1.2.3-non-installer.zip`

If you built with `RelWithDebInfo` instead, pass `-BuildDir ..\..\build_x64\rundir\RelWithDebInfo`.

## macOS

Firstly install prerequisites based on [Build Instructions For Mac](https://github.com/obsproject/obs-studio/wiki/Build-Instructions-For-Mac).

* macOS 14.1 (minimum: macOS 13.5)
* Xcode 15.4
* CMake 3.30 (minimum: CMake 3.28)
* CCache 4.8 or newer (Optional)

Then build the plugin (default preset builds a Universal binary):

```bash
cmake --preset macos
cmake --build --preset macos --config RelWithDebInfo

# Enable YouTube explicitly when needed
cmake --preset macos -DENABLE_YOUTUBE=ON
cmake --build --preset macos --config RelWithDebInfo
```

Then open the generated Xcode project `build_macos/obs-17live.xcodeproj`. Build the plugin.

### Packaging (manual)

Build a `.pkg` (installer) and a `-non-installer.zip` locally:

```bash
CONFIG=Release

cmake --build --preset macos --config "$CONFIG"

# Enable YouTube explicitly when needed
cmake --preset macos -DENABLE_YOUTUBE=ON
cmake --build --preset macos --config "$CONFIG"

INSTALL_PREFIX="$PWD/dist-install"
rm -rf "$INSTALL_PREFIX"
cmake --install build_macos --config "$CONFIG" --prefix "$INSTALL_PREFIX"

# Output pkg:
#   dist-install/obs-17live.pkg

PLUGIN_BUNDLE="build_macos/rundir/$CONFIG/obs-17live.plugin"
ditto -c -k --sequesterRsrc --keepParent "$PLUGIN_BUNDLE" "obs-17live-non-installer.zip"
```

The `.pkg` installs into:

- `~/Library/Application Support/obs-studio/plugins`

The zip contains `obs-17live.plugin`; extract and copy it into the same directory above.

## CI / Release Packaging

Detailed CI variable / environment setup is documented in `docs/ci-configuration.md`.

- There are no `*-prod` presets. CI uses the same presets and injects environment-specific values
  via `-D` arguments and GitHub Actions environment variables.
- The Steam version of OBS is essentially OBS Studio. To ensure compatibility with both the official and Steam versions, the installer no longer relies on OBS’s installation directory; instead, it installs into OBS’s user plugin directory (which OBS automatically scans).
- Key injected variables:
  - `ONESEVENLIVE_API_URL` (GitHub Actions env/vars)
  - `CMAKE_PROJECT_VERSION` (derived from git tag or workflow input)
  - `YOUTUBE_API_CLIENT_ID`, `YOUTUBE_API_CLIENT_SECRET`, `TWITCH_API_CLIENT_ID` (vars)
  - `ENABLE_YOUTUBE` (`ON` to enable, otherwise defaults to `OFF`)

```bash
cmake --preset macos \
  -DENABLE_YOUTUBE="$ENABLE_YOUTUBE" \
  -DYOUTUBE_API_CLIENT_ID="$YOUTUBE_API_CLIENT_ID" \
  -DYOUTUBE_API_CLIENT_SECRET="$YOUTUBE_API_CLIENT_SECRET" \
  -DTWITCH_API_CLIENT_ID="$TWITCH_API_CLIENT_ID" \
  -DONESEVENLIVE_API_URL="$ONESEVENLIVE_API_URL" \
  -DCMAKE_PROJECT_VERSION="$VERSION"

cd web/ably_chat
ENABLE_YOUTUBE="${ENABLE_YOUTUBE:-false}" npm run build
cd ../..

cmake --build --preset macos --config Release

cmake --install build_macos --config Release --prefix "$PWD/dist-install"
# macOS pkg: dist-install/obs-17live.pkg
# CI also exports non-installer zip containing:
#   obs-17live.plugin
# copy this bundle directly into:
#   ~/Library/Application Support/obs-studio/plugins

cmake --preset windows-x64 ^
  -DENABLE_YOUTUBE="%ENABLE_YOUTUBE%" ^
  -DYOUTUBE_API_CLIENT_ID="%YOUTUBE_API_CLIENT_ID%" ^
  -DYOUTUBE_API_CLIENT_SECRET="%YOUTUBE_API_CLIENT_SECRET%" ^
  -DTWITCH_API_CLIENT_ID="%TWITCH_API_CLIENT_ID%" ^
  -DONESEVENLIVE_API_URL="%ONESEVENLIVE_API_URL%" ^
  -DCMAKE_PROJECT_VERSION="%VERSION%"

cd web/ably_chat
set ENABLE_YOUTUBE=%ENABLE_YOUTUBE%
npm run build
cd ..\..

cmake --build --preset windows-x64 --config Release

# Windows installer is built from package/windows and installs into:
# %ProgramData%\obs-studio\plugins\obs-17live
# CI also exports non-installer zip containing:
#   obs-17live/bin/64bit/obs-17live.dll
#   obs-17live/data/...
# copy the extracted obs-17live folder directly into:
#   %ProgramData%\obs-studio\plugins
```

### Optional: macOS legacy uninstaller pkg (one-time)

If you previously installed this plugin into the legacy location inside the OBS app bundle:

- `/Applications/OBS.app/Contents/PlugIns/obs-17live.plugin`

You can generate a small `uninstall-legacy.pkg` that removes it and shows a completion dialog.

```bash
VERSION="0.0.0"
bash ./package/macOS/build-uninstall-legacy-pkg.sh "$VERSION" "obs-17live-uninstall-legacy.pkg"
```

Then double-click `obs-17live-uninstall-legacy.pkg` to run it.
