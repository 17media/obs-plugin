# Build OBS 17LIVE Plugin

This guide will walk you through the process of building the OBS 17LIVE plugin from source.

## Windows x64

Mainly based on [Build OBS Plugin](https://github.com/obsproject/obs-studio/wiki/build-instructions-for-windows).

Then build the plugin:

```bash
cmake --preset windows-x64
```

Then open the generated solution file in Visual Studio. Build the plugin.

## MacOS

Mainly based on [Build OBS Plugin](https://github.com/obsproject/obs-studio/wiki/Build-Instructions-For-Mac).

Then build the plugin for Apple Silicon:

```bash
cmake --preset macos-arm64
```

or for Intel:

```bash
cmake --preset macos-x86_64
```

Then open the generated Xcode project. Build the plugin.
