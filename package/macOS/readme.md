# package for macOS

```
cd {project_base_path}

sudo pkgbuild --root build_macos/Debug --identifier com.17live.obsplugin --version 1.0 --install-location "/Applications/OBS.app/Contents/PlugIns" --scripts package/macOS/misc dist/Obs17Live_MacOS_AppleSilicon_0609.pkg

sudo pkgbuild --root build_macos/Debug --identifier com.17live.obsplugin --version 1.0 --install-location "/Applications/OBS.app/Contents/PlugIns" --scripts package/macOS/misc dist/Obs17Live_MacOS_Intel_0616.pkg
```