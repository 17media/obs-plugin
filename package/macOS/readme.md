# package for macOS

```
cd {project_base_path}

sudo pkgbuild --root build_macos/Debug --identifier com.17live.obsplugin --version 1.0 --install-location "/Applications/OBS.app/Contents/PlugIns" --scripts package/macOS/misc dist/17liveOBSPlugin-macAppleSilicon-v0.2.0.pkg

sudo pkgbuild --root build_macos/Debug --identifier com.17live.obsplugin --version 1.0 --install-location "/Applications/OBS.app/Contents/PlugIns" --scripts package/macOS/misc dist/17liveOBSPlugin-macIntel-v0.2.0.pkg
```