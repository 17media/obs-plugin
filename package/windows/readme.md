# package for windows installer

We use [HeatWave for VS2022](https://marketplace.visualstudio.com/items?itemName=FireGiant.FireGiantHeatWaveDev17) to generate the installer.

## gather data files into Obs17livePluginData.wxs

use [heat](https://github.com/wixtoolset/wix/releases/tag/v6.0.1) to gather data files into Obs17livePluginData.wxs

```
cd {project_base_path}

heat dir data -cg Obs17livePluginData -dr DATAFOLDER -gg -g1 -sfrag -srd -out package\windows\Obs17Live_Windows_x64_Installer\Obs17livePluginData.wxs
```

replace `SourceDir` in Obs17livePluginData.wxs with the correct path, which should be `{project_base_path}\data`

## build msi

build in Obs17Live_Windows_x64_Installer project

**Note: update version number in Package.wxs to replace the old ones when installing**

## build exe installer

build in Obs17Live project