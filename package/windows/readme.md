# package for windows installer

We use [HeatWave for VS2022](https://marketplace.visualstudio.com/items?itemName=FireGiant.FireGiantHeatWaveDev17) to generate the installer.

## gather data files into Obs17livePluginData.wxs

```
heat dir {project_base_path}\obs-17live\data -cg Obs17livePluginData -dr DATAFOLDER -gg -g1 -sfrag -srd -out {project_base_path}\package\windows\Obs17Live_Windows_x64_Installer\Obs17livePluginData.wxs
```

replace `SourceDir` in Obs17livePluginData.wxs with the correct path, which should be `{project_base_path}\obs-17live\data`

## build msi

build in Obs17Live_Windows_x64_Installer project

**Note: update version number in Package.wxs to replace the old ones when installing**

## build exe installer

build in Obs17Live project