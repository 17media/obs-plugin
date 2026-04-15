# package for macOS

This project ships a `.pkg` (requires admin password) that installs the plugin into the OBS user plugin directory:

`~/Library/Application Support/obs-studio/plugins`

That directory is shared by both the official OBS build and the Steam OBS build, so the same installer works for both.

The installer also removes legacy installs inside the official OBS.app bundle:

`/Applications/OBS.app/Contents/PlugIns/obs-17live.plugin`

```bash
cd {project_base_path}

cmake --preset macos
cmake --build --preset macos --config Release

INSTALL_PREFIX="$PWD/dist-install"
rm -rf "$INSTALL_PREFIX"
cmake --install build_macos --config Release --prefix "$INSTALL_PREFIX"

ls -la "$INSTALL_PREFIX/obs-17live.pkg"
```
